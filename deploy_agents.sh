#!/usr/bin/env bash

# Файл со списком хостов и типом пакета
HOSTS_FILE="deploy_agents.conf"

# Путь к локальным пакетам
AGENT_DEB="./agent.deb"
AGENT_RPM="./agent.rpm"

# Дополнительные настройки, которые будем указывать в конфиге
URL="http://192.168.160.37:5050/report"
TIME="30"
MOUNT="/"

# Имя сервера (локальное)
SERVER_NAME="$(hostname)"

# Открываем конфиг во входном потоке дескриптора 3
exec 3< "$HOSTS_FILE"

# Читаем построчно именно из дескриптора 3
while IFS= read -r line <&3; do
    # Пропускаем пустые строки и комментарии
    [[ -z "$line" || "$line" =~ ^# ]] && continue

    # Извлекаем IP, пользователя, тип пакета
    IP=$(echo "$line" | awk '{print $1}')
    USERNAME=$(echo "$line" | awk '{print $2}')
    PKG_TYPE=$(echo "$line" | awk '{print $3}')

    echo "-----------------------------------------"
    echo "Обрабатываем хост: $IP, пользователь: $USERNAME, тип пакета: $PKG_TYPE"
    echo "-----------------------------------------"

    # Определяем вторую сетевую карту (кроме lo)
    INTERFACE=$(ssh -o StrictHostKeyChecking=no "$USERNAME@$IP" \
        "ip -o link show | awk '{print \$2}' | sed 's/://' | grep -v '^lo\$' | sed -n '1p'")

    if [[ -z "$INTERFACE" ]]; then
        echo "Не удалось определить вторую сетевую карту на $IP. Пропускаем..."
        continue
    fi

    case "$PKG_TYPE" in
        deb)
            echo "Копируем deb-пакет на $IP и устанавливаем..."
            scp -o StrictHostKeyChecking=no "$AGENT_DEB" "$USERNAME@$IP:/tmp/agent.deb"

            # Установка deb, передаём переменные окружения
            ssh -o StrictHostKeyChecking=no "$USERNAME@$IP" sudo \
                SERVER_NAME="$SERVER_NAME" \
                INTERFACE="$INTERFACE" \
                MOUNT="$MOUNT" \
                URL="$URL" \
                TIME="$TIME" \
                dpkg -i /tmp/agent.deb
            ;;
        rpm)
            echo "Копируем rpm-пакет на $IP и устанавливаем..."
            scp -o StrictHostKeyChecking=no "$AGENT_RPM" "$USERNAME@$IP:/tmp/agent.rpm"

            # Устанавливаем или обновляем RPM
            ssh -o StrictHostKeyChecking=no "$USERNAME@$IP" sudo \
                rpm -Uvh --force /tmp/agent.rpm

            # Перезаписываем /etc/monitoring_agent.conf вручную
            ssh -o StrictHostKeyChecking=no "$USERNAME@$IP" sudo bash -c "cat > /etc/monitoring_agent.conf <<EOF
SERVER_NAME=\"$SERVER_NAME\"
INTERFACE=\"$INTERFACE\"
MOUNT=\"$MOUNT\"
URL=\"$URL\"
TIME=\"$TIME\"
EOF
systemctl daemon-reload
systemctl enable agent.service
systemctl restart agent.service
"
            ;;
        *)
            echo "Неизвестный тип пакета: $PKG_TYPE. Пропускаем..."
            continue
            ;;
    esac

    echo "Установка/обновление агента на $IP завершена."
done

# Закрываем дескриптор 3 (необязательно, но для порядка)
exec 3<&-
