i=0

while true
do
    ls |while read file; do
        case "$file" in 
        *$2*)
            echo "Exec $1 < $file"
            $1 < $file
            sleep 10
            ;;
        esac
    done
    echo "Round $i"
    sleep 10
done