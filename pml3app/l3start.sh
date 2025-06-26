if [ -f "${HOME}/l3_log_mode" ]; then
    mode=$(cat "${HOME}/l3_log_mode")
    case "$mode" in
        0)
            echo "Log mode is 0 - Using debug config"
            cp "${HOME}/log4crc.debug" "${HOME}/log4crc"
            ;;
        1)
            echo "Log mode is 1 - Using info config"
            cp "${HOME}/log4crc.info" "${HOME}/log4crc"
            ;;
        2)
            echo "Log mode is 2 - Using error config"
            cp "${HOME}/log4crc.error" "${HOME}/log4crc"
            ;;
        *)
            echo "Unknown log mode: $mode"
            ;;
    esac
else
    echo "Log mode file not found, using default log4crc"
fi
export LOG4C_RCPATH=${HOME}/log4crc
./pml3