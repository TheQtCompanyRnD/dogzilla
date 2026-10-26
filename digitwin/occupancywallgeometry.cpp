// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "occupancywallgeometry.h"

#include <QtCore/qloggingcategory.h>
#include <QtCore/qmath.h>
#include <QtGui/qpainterpath.h>
#include <QtGui/qvector2d.h>
#include <QtGui/qvector3d.h>
#include <QtGui/qvector4d.h>
#include <QtGui/private/qtriangulator_p.h>

#include <array>
#include <vector>

Q_STATIC_LOGGING_CATEGORY(lcWalls, "dogzilla.walls")

namespace {

// The occupancy grid is blurred with a [1 2 1]/4 kernel in each direction
// before contouring. A single isolated cell then peaks at 0.25 (and disappears),
// a one-cell-wide diagonal line peaks at 0.375 (and survives), and the
// edge of a large solid region ends up ~0.45 cells outside the original cell boundary.
constexpr float IsoLevel = 0.28f;
// Margin of empty cells around the grid, so that every contour is closed.
constexpr int Pad = 2;
// Contours enclosing less than this many square cells are discarded as noise.
constexpr qreal MinAreaCells = 1.0;
// Vertex color at the foot of a wall (1 at the top): fake ambient occlusion,
// so that faces turned away from the light don't go flat black.
constexpr float FootShade = 0.55f;

struct Vertex {
    QVector3D position;
    QVector3D normal;
    QVector4D color;
    QVector2D uv;
    QVector3D tangent;
    QVector3D binormal;
};

qreal signedArea(const QList<QPointF> &loop)
{
    qreal a = 0;
    for (qsizetype i = 0, n = loop.size(); i < n; ++i) {
        const QPointF &p = loop.at(i);
        const QPointF &q = loop.at((i + 1) % n);
        a += p.x() * q.y() - q.x() * p.y();
    }
    return a / 2;
}

qreal distanceToSegmentSquared(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const qreal len2 = QPointF::dotProduct(ab, ab);
    qreal t = len2 > 0 ? QPointF::dotProduct(p - a, ab) / len2 : 0;
    t = qBound(0.0, t, 1.0);
    const QPointF d = p - (a + t * ab);
    return QPointF::dotProduct(d, d);
}

/*
    Douglas-Peucker simplification of a closed loop: split it at the first
    point and the point farthest from it, and simplify both halves.
*/
QList<QPointF> simplifyLoop(const QList<QPointF> &loop, qreal tolerance)
{
    const qsizetype n = loop.size();
    if (n < 4 || tolerance <= 0)
        return loop;

    qsizetype farthest = 0;
    qreal farthestDist2 = -1;
    for (qsizetype i = 1; i < n; ++i) {
        const QPointF d = loop.at(i) - loop.first();
        const qreal d2 = QPointF::dotProduct(d, d);
        if (d2 > farthestDist2) {
            farthestDist2 = d2;
            farthest = i;
        }
    }

    // index n is loop[0] again
    auto at = [&](qsizetype i) -> const QPointF & { return loop.at(i % n); };
    std::vector<bool> keep(n + 1, false);
    keep[0] = keep[farthest] = keep[n] = true;
    const qreal tol2 = tolerance * tolerance;
    std::vector<std::pair<qsizetype, qsizetype>> stack { { 0, farthest }, { farthest, n } };
    while (!stack.empty()) {
        const auto [first, last] = stack.back();
        stack.pop_back();
        qsizetype worst = -1;
        qreal worstDist2 = tol2;
        for (qsizetype i = first + 1; i < last; ++i) {
            const qreal d2 = distanceToSegmentSquared(at(i), at(first), at(last));
            if (d2 > worstDist2) {
                worstDist2 = d2;
                worst = i;
            }
        }
        if (worst >= 0) {
            keep[worst] = true;
            stack.push_back({ first, worst });
            stack.push_back({ worst, last });
        }
    }

    QList<QPointF> ret;
    for (qsizetype i = 0; i < n; ++i) {
        if (keep[i])
            ret.append(loop.at(i));
    }
    return ret;
}

} // namespace

/*!
    \qmltype OccupancyWallGeometry
    \brief Procedural geometry for the obstacles in an occupancy grid.

    The cells in which \c {probability >= threshold} are blurred slightly,
    outlined with marching squares, and the outlines are simplified so that
    straight walls become single long segments while curved furniture keeps
    as many segments as it needs to stay within \l tolerance. Each outline is
    then extruded upwards to \l wallHeight, and the tops are capped.

    In \l {mode}{Floor} mode, the same machinery outlines all the cells that
    the map has seen (known, whether free or occupied), and fills them to make
    a flat, textured surface. Walls stand on its edge where they enclose it.
*/
OccupancyWallGeometry::OccupancyWallGeometry(QQuick3DObject *parent) : QQuick3DGeometry(parent)
{
}

/*!
    \qmlproperty enumeration OccupancyWallGeometry::mode

    \value Walls   Extrude the obstacle outlines into walls (the default).
    \value Floor   Fill the area that the map has seen (free or occupied) at height 0.
*/
void OccupancyWallGeometry::setMode(Mode newMode)
{
    if (m_mode == newMode)
        return;
    m_mode = newMode;
    scheduleRebuild();
    emit modeChanged();
}

/*!
    \qmlproperty real OccupancyWallGeometry::uvSize

    In Floor mode, the size in meters that the texture coordinates 0 to 1
    span. The default is 1.
*/
void OccupancyWallGeometry::setUvSize(qreal newUvSize)
{
    if (qFuzzyCompare(m_uvSize, float(newUvSize)))
        return;
    m_uvSize = newUvSize;
    scheduleRebuild();
    emit uvSizeChanged();
}

/*!
    \qmlproperty size OccupancyWallGeometry::size

    This property holds the size of the map in \l resolution units.
*/
void OccupancyWallGeometry::setSize(const QSize &newSize)
{
    if (m_size == newSize)
        return;
    m_size = newSize;
    scheduleRebuild();
    emit sizeChanged();
}

/*!
    \qmlproperty real OccupancyWallGeometry::resolution

    This property holds the resolution of the map in meters per grid square.
    The default is 0.05 (5 cm).
*/
void OccupancyWallGeometry::setResolution(qreal newResolution)
{
    if (qFuzzyCompare(m_resolution, float(newResolution)))
        return;
    m_resolution = newResolution;
    scheduleRebuild();
    emit resolutionChanged();
}

/*!
    \qmlproperty QByteArray OccupancyWallGeometry::probabilities

    This property holds the array of probabilities in the map's grid squares,
    as signed bytes in row-major order: -1 for unknown, otherwise 0 to 100.
*/
void OccupancyWallGeometry::setProbabilities(const QByteArray &newProbabilities)
{
    if (m_probabilities == newProbabilities)
        return;
    m_probabilities = newProbabilities;
    scheduleRebuild();
    emit probabilitiesChanged();
}

/*!
    \qmlproperty int OccupancyWallGeometry::threshold

    The minimum probability (0 to 100) for a cell to count as an obstacle.
    The default is 50.
*/
void OccupancyWallGeometry::setThreshold(int newThreshold)
{
    if (m_threshold == newThreshold)
        return;
    m_threshold = newThreshold;
    scheduleRebuild();
    emit thresholdChanged();
}

/*!
    \qmlproperty real OccupancyWallGeometry::wallHeight

    The height of the extruded walls in meters. The default is 0.25.
*/
void OccupancyWallGeometry::setWallHeight(qreal newWallHeight)
{
    if (qFuzzyCompare(m_wallHeight, float(newWallHeight)))
        return;
    m_wallHeight = newWallHeight;
    scheduleRebuild();
    emit wallHeightChanged();
}

/*!
    \qmlproperty real OccupancyWallGeometry::tolerance

    How far (in meters) a simplified outline may deviate from the traced one.
    Larger values give straighter, cleaner walls; smaller values follow the
    map more faithfully. The default is 0.025.
*/
void OccupancyWallGeometry::setTolerance(qreal newTolerance)
{
    if (qFuzzyCompare(m_tolerance, float(newTolerance)))
        return;
    m_tolerance = newTolerance;
    scheduleRebuild();
    emit toleranceChanged();
}

/*!
    \qmlproperty real OccupancyWallGeometry::creaseAngle

    Where two outline segments meet at less than this angle (in degrees),
    their normals are averaged so that the surface is shaded smoothly
    (curved furniture); at sharper angles the corner stays hard (wall
    corners). The default is 35.
*/
void OccupancyWallGeometry::setCreaseAngle(qreal newCreaseAngle)
{
    if (qFuzzyCompare(m_creaseAngle, float(newCreaseAngle)))
        return;
    m_creaseAngle = newCreaseAngle;
    scheduleRebuild();
    emit creaseAngleChanged();
}

/*
    The properties typically change together when a map message arrives;
    rebuild only once afterwards.
*/
void OccupancyWallGeometry::scheduleRebuild()
{
    if (m_rebuildPending)
        return;
    m_rebuildPending = true;
    QMetaObject::invokeMethod(this, &OccupancyWallGeometry::rebuild, Qt::QueuedConnection);
}

void OccupancyWallGeometry::rebuild()
{
    m_rebuildPending = false;

    const int w = m_size.width();
    const int h = m_size.height();
    if (w <= 0 || h <= 0 || m_probabilities.size() != qsizetype(w) * h)
        return; // probably size and data are out of sync: wait for the next change

    // Outlines of the regions of cells for which isSolid(probability) is true,
    // each oriented so that the region is on its left.
    auto traceLoops = [&](auto isSolid) {
        // Threshold into a padded grid, then blur.
        const int pw = w + 2 * Pad;
        const int ph = h + 2 * Pad;
        std::vector<float> field(size_t(pw) * ph, 0.0f);
        for (int r = 0; r < h; ++r) {
            for (int c = 0; c < w; ++c) {
                if (isSolid(m_probabilities.at(r * w + c)))
                    field[(r + Pad) * pw + c + Pad] = 1.0f;
            }
        }
        std::vector<float> tmp(field.size(), 0.0f);
        for (int r = 0; r < ph; ++r) {
            for (int c = 1; c < pw - 1; ++c) {
                const int i = r * pw + c;
                tmp[i] = 0.25f * field[i - 1] + 0.5f * field[i] + 0.25f * field[i + 1];
            }
        }
        for (int r = 1; r < ph - 1; ++r) {
            for (int c = 0; c < pw; ++c) {
                const int i = r * pw + c;
                field[i] = 0.25f * tmp[i - pw] + 0.5f * tmp[i] + 0.25f * tmp[i + pw];
            }
        }
        auto inside = [&](int c, int r) { return field[r * pw + c] > IsoLevel; };

        // Marching squares. Each grid edge that the contour crosses gets an id:
        // 2 * sampleIndex for the edge to the right of that sample, +1 for the
        // edge below it. Each crossing joins exactly two segments, so the
        // segments link up into closed loops.
        std::vector<std::array<int, 2>> links(field.size() * 2, { -1, -1 });
        auto link = [&](int a, int b) {
            links[a][links[a][0] < 0 ? 0 : 1] = b;
            links[b][links[b][0] < 0 ? 0 : 1] = a;
        };
        for (int r = 0; r < ph - 1; ++r) {
            for (int c = 0; c < pw - 1; ++c) {
                const bool in0 = inside(c, r);         // top left
                const bool in1 = inside(c + 1, r);     // top right
                const bool in2 = inside(c + 1, r + 1); // bottom right
                const bool in3 = inside(c, r + 1);     // bottom left
                const int e0 = 2 * (r * pw + c);           // top
                const int e1 = 2 * (r * pw + c + 1) + 1;   // right
                const int e2 = 2 * ((r + 1) * pw + c);     // bottom
                const int e3 = 2 * (r * pw + c) + 1;       // left
                const int crossings = (in0 != in1) + (in1 != in2) + (in2 != in3) + (in3 != in0);
                if (crossings == 2) {
                    int ends[2];
                    int n = 0;
                    if (in0 != in1) ends[n++] = e0;
                    if (in1 != in2) ends[n++] = e1;
                    if (in2 != in3) ends[n++] = e2;
                    if (in3 != in0) ends[n++] = e3;
                    link(ends[0], ends[1]);
                } else if (crossings == 4) {
                    // saddle: the center decides which pair of corners is connected
                    const float center = 0.25f * (field[r * pw + c] + field[r * pw + c + 1] +
                                                  field[(r + 1) * pw + c] + field[(r + 1) * pw + c + 1]);
                    if ((center > IsoLevel) == in0) {
                        link(e0, e1); // cut off corner 1
                        link(e2, e3); // cut off corner 3
                    } else {
                        link(e3, e0); // cut off corner 0
                        link(e1, e2); // cut off corner 2
                    }
                }
            }
        }

        // Crossing position in grid cell units (cell (c, r) is centered at (c + 0.5, r + 0.5))
        auto crossingPoint = [&](int id) {
            const int s = id / 2;
            const int c = s % pw;
            const int r = s / pw;
            const int c2 = (id & 1) ? c : c + 1;
            const int r2 = (id & 1) ? r + 1 : r;
            const float fa = field[r * pw + c];
            const float fb = field[r2 * pw + c2];
            const float t = qBound(0.0f, (IsoLevel - fa) / (fb - fa), 1.0f);
            return QPointF(c + t * (c2 - c) - Pad + 0.5, r + t * (r2 - r) - Pad + 0.5);
        };
        // Bilinear sample of the blurred field, in grid cell units
        auto fieldAt = [&](QPointF p) {
            const qreal x = qBound(0.0, p.x() + Pad - 0.5, pw - 1.001);
            const qreal y = qBound(0.0, p.y() + Pad - 0.5, ph - 1.001);
            const int c = int(x);
            const int r = int(y);
            const float fx = x - c;
            const float fy = y - r;
            const float top = field[r * pw + c] * (1 - fx) + field[r * pw + c + 1] * fx;
            const float bottom = field[(r + 1) * pw + c] * (1 - fx) + field[(r + 1) * pw + c + 1] * fx;
            return top * (1 - fy) + bottom * fy;
        };

        QList<QList<QPointF>> loops;
        std::vector<bool> visited(links.size(), false);
        const qreal toleranceCells = m_resolution > 0 ? m_tolerance / m_resolution : 0;
        for (int start = 0; start < int(links.size()); ++start) {
            if (visited[start] || links[start][0] < 0)
                continue;
            QList<QPointF> loop;
            int prev = -1;
            int cur = start;
            do {
                visited[cur] = true;
                loop.append(crossingPoint(cur));
                const int next = links[cur][0] != prev ? links[cur][0] : links[cur][1];
                prev = cur;
                cur = next;
            } while (cur != start && cur >= 0);
            if (cur < 0 || qAbs(signedArea(loop)) < MinAreaCells)
                continue;

            // Orient each loop so that the obstacle is on its left: sample the
            // field just to the left of the middle of its longest segment.
            qsizetype longest = 0;
            qreal longestLen2 = -1;
            for (qsizetype i = 0; i < loop.size(); ++i) {
                const QPointF d = loop.at((i + 1) % loop.size()) - loop.at(i);
                const qreal len2 = QPointF::dotProduct(d, d);
                if (len2 > longestLen2) {
                    longestLen2 = len2;
                    longest = i;
                }
            }
            const QPointF a = loop.at(longest);
            const QPointF d = (loop.at((longest + 1) % loop.size()) - a) / qSqrt(longestLen2);
            if (fieldAt((a + loop.at((longest + 1) % loop.size())) / 2 + 0.2 * QPointF(-d.y(), d.x())) <= IsoLevel)
                std::reverse(loop.begin(), loop.end());

            loop = simplifyLoop(loop, toleranceCells);
            if (loop.size() >= 3)
                loops.append(loop);
        }
        return loops;
    };

    const QList<QList<QPointF>> loops = m_mode == Floor
        ? traceLoops([](qint8 p) { return p >= 0; })           // everything the map has seen
        : traceLoops([this](qint8 p) { return p >= m_threshold; });

    // Build the mesh. Grid (column, row) maps to scene (Z, X), in meters.
    const float res = m_resolution;
    const float top = m_wallHeight;
    auto toScene = [res](QPointF p, float y) { return QVector3D(p.y() * res, y, p.x() * res); };
    auto outwardNormal = [](QPointF from, QPointF to) {
        // obstacle is on the left, so outward is to the right: (dy, -dx) in grid space
        const QPointF d = to - from;
        return QVector3D(-d.x(), 0, d.y()).normalized();
    };
    const float cosCrease = qCos(qDegreesToRadians(m_creaseAngle));
    const QVector4D white(1, 1, 1, 1);
    const QVector4D foot(FootShade, FootShade, FootShade, 1);

    std::vector<Vertex> vertices;
    std::vector<quint32> indices;
    auto addTriangle = [&](quint32 a, quint32 b, quint32 c, const QVector3D &faceNormal) {
        const QVector3D cross = QVector3D::crossProduct(vertices[b].position - vertices[a].position,
                                                        vertices[c].position - vertices[a].position);
        if (QVector3D::dotProduct(cross, faceNormal) < 0)
            std::swap(b, c); // counter-clockwise when seen from outside
        indices.insert(indices.end(), { a, b, c });
    };

    // Triangulate a flat, upward-facing surface at the given height
    auto addCaps = [&](const QPainterPath &path, float y) {
        if (path.isEmpty())
            return;
        const QTriangleSet caps = qTriangulate(path);
        const QVector3D up(0, 1, 0);
        const quint32 base = quint32(vertices.size());
        for (qsizetype i = 0; i + 1 < caps.vertices.size(); i += 2) {
            const QVector3D p = toScene(QPointF(caps.vertices.at(i), caps.vertices.at(i + 1)), y);
            // tangent space for normal mapping: u along scene X, v along scene Z
            vertices.push_back({ p, up, white, QVector2D(p.x(), p.z()) / m_uvSize,
                                 QVector3D(1, 0, 0), QVector3D(0, 0, -1) });
        }
        const int count = caps.indices.size();
        const bool uint32 = caps.indices.type() == QVertexIndexVector::UnsignedInt;
        auto index = [&](int i) -> quint32 {
            return base + (uint32 ? static_cast<const quint32 *>(caps.indices.data())[i]
                                  : static_cast<const quint16 *>(caps.indices.data())[i]);
        };
        for (int i = 0; i + 2 < count; i += 3)
            addTriangle(index(i), index(i + 1), index(i + 2), up);
    };

    if (m_mode == Floor) {
        // The known area, with holes where there are unknown pockets. It reaches
        // under the walls, which hide its edge where they enclose it.
        QPainterPath floorPath;
        floorPath.setFillRule(Qt::OddEvenFill);
        for (const QList<QPointF> &loop : std::as_const(loops)) {
            floorPath.moveTo(loop.first());
            for (qsizetype i = 1; i < loop.size(); ++i)
                floorPath.lineTo(loop.at(i));
            floorPath.closeSubpath();
        }
        addCaps(floorPath, 0);
    } else {
        QPainterPath capPath;
        capPath.setFillRule(Qt::OddEvenFill);
        for (const QList<QPointF> &loop : std::as_const(loops)) {
            const qsizetype n = loop.size();
            std::vector<QVector3D> faceNormals(n);
            for (qsizetype i = 0; i < n; ++i)
                faceNormals[i] = outwardNormal(loop.at(i), loop.at((i + 1) % n));
            // the normal at the start of segment i, shared with the end of segment i - 1 if smooth
            auto cornerNormal = [&](qsizetype i) {
                const QVector3D &before = faceNormals[(i + n - 1) % n];
                const QVector3D &after = faceNormals[i];
                return QVector3D::dotProduct(before, after) >= cosCrease ? (before + after).normalized() : QVector3D();
            };
            for (qsizetype i = 0; i < n; ++i) {
                const qsizetype j = (i + 1) % n;
                const QVector3D &faceNormal = faceNormals[i];
                QVector3D na = cornerNormal(i);
                QVector3D nb = cornerNormal(j);
                if (na.isNull())
                    na = faceNormal;
                if (nb.isNull())
                    nb = faceNormal;
                const quint32 base = quint32(vertices.size());
                vertices.push_back({ toScene(loop.at(i), 0), na, foot });
                vertices.push_back({ toScene(loop.at(i), top), na, white });
                vertices.push_back({ toScene(loop.at(j), 0), nb, foot });
                vertices.push_back({ toScene(loop.at(j), top), nb, white });
                addTriangle(base, base + 2, base + 3, faceNormal);
                addTriangle(base, base + 3, base + 1, faceNormal);
            }

            capPath.moveTo(loop.first());
            for (qsizetype i = 1; i < n; ++i)
                capPath.lineTo(loop.at(i));
            capPath.closeSubpath();
        }

        // Cap the tops. Loops around solid regions and around holes in them
        // (e.g. a room enclosed by walls) nest properly, so odd-even fill works.
        addCaps(capPath, top);
    }

    clear();
    m_contourCount = loops.size();
    m_vertexCount = int(vertices.size());
    if (!vertices.empty()) {
        QVector3D minBound = vertices.front().position;
        QVector3D maxBound = minBound;
        for (const Vertex &v : vertices) {
            minBound = QVector3D(qMin(minBound.x(), v.position.x()), qMin(minBound.y(), v.position.y()),
                                 qMin(minBound.z(), v.position.z()));
            maxBound = QVector3D(qMax(maxBound.x(), v.position.x()), qMax(maxBound.y(), v.position.y()),
                                 qMax(maxBound.z(), v.position.z()));
        }
        setStride(sizeof(Vertex));
        setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
        addAttribute(QQuick3DGeometry::Attribute::PositionSemantic, offsetof(Vertex, position),
                     QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::NormalSemantic, offsetof(Vertex, normal),
                     QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::ColorSemantic, offsetof(Vertex, color),
                     QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::TexCoord0Semantic, offsetof(Vertex, uv),
                     QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::TangentSemantic, offsetof(Vertex, tangent),
                     QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::BinormalSemantic, offsetof(Vertex, binormal),
                     QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::IndexSemantic, 0, QQuick3DGeometry::Attribute::U32Type);
        setVertexData(QByteArray(reinterpret_cast<const char *>(vertices.data()),
                                 qsizetype(vertices.size() * sizeof(Vertex))));
        setIndexData(QByteArray(reinterpret_cast<const char *>(indices.data()),
                                qsizetype(indices.size() * sizeof(quint32))));
        setBounds(minBound, maxBound);
    }
    qCDebug(lcWalls) << w << "x" << h << "grid ->" << m_contourCount << "contours," << m_vertexCount
                     << "vertices," << indices.size() / 3 << "triangles";
    update();
    emit geometryRebuilt();
}
