#include "receivedimageprovider.h"
#include <QtCore/qtsymbolmacros.h>
#include <QtQml/qqmlextensionplugin.h>

QT_DECLARE_EXTERN_SYMBOL_VOID(qml_register_types_Dogzilla)

class DogzillaPlugin : public QQmlEngineExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlEngineExtensionInterface_iid)
public:
    DogzillaPlugin(QObject *parent = nullptr) : QQmlEngineExtensionPlugin(parent)
    {
        QT_KEEP_SYMBOL(qml_register_types_Dogzilla)
    }
    void initializeEngine(QQmlEngine *engine, const char *uri) override
    {
        Q_UNUSED(uri)
        engine->addImageProvider(QLatin1String("camera"), ReceivedImageProvider::instance());
    }
};

#include "dogzillaplugin.moc"
