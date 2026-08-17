# Processes by resident memory, largest first. `rss`, `rss 30`, `rss pipewire`.
#
# Worth having because neither BusyBox tool answers "what's using the RAM?":
# this build's `top` is VSZ-only (no -m), and VSZ is mapped address space, not
# memory -- dogzillad reads 2477m there against ~230 MB resident, and an idle
# fish reads 226m against under 9 MB. `ps -o pid,rss,comm` does exist, but it
# prints "10m"-style values that won't sort numerically. So read VmRSS out of
# /proc directly. Kernel threads have no VmRSS and drop out on their own.
#
# RSS double-counts shared pages (the pipewire processes share plenty), so treat
# it as an order of magnitude, not an audit.
function rss --description 'Processes by resident memory (RSS), largest first'
    set -l count 15
    set -l pattern

    for arg in $argv
        if string match -qr '^[0-9]+$' -- $arg
            set count $arg
        else
            set pattern $arg
        end
    end

    # fish globs don't do bracket character classes, so /proc/[0-9]*/status is
    # not a thing here -- pick the numeric entries out of /proc by hand.
    set -l files (printf '/proc/%s/status\n' (ls /proc | string match -r '^[0-9]+$'))

    set -l rows (awk '/^Name:/{n=$2} /^Pid:/{p=$2} /^VmRSS:/{printf "%9.1f MB %7d  %s\n", $2/1024, p, n}' $files 2>/dev/null | sort -rn)

    if set -q pattern[1]
        set rows (printf '%s\n' $rows | grep -i -- $pattern)
    end

    if set -q rows[1]
        printf '%s\n' $rows | head -n $count
    end

    free -m | awk '/^Mem:/{printf "  in total: %d MB used of %d MB\n", $3, $2}'
end
