#!/bin/bash
# Скрипт для обновления и перезапуска сервера на тестовом стенде (с использованием git stash)

HOST="192.168.160.37"
USER="root"

REMOTE_DIR="/home/hacker/monitoring"
NEW_PORT="5050"

ssh "${USER}@${HOST}" << EOF

  echo "Переходим в директорию проекта..."
  cd "${REMOTE_DIR}"

  echo "Сохраняем локальные изменения в stash..."
  git stash

  echo "Обновляем репозиторий..."
  git pull

  echo "Восстанавливаем локальные изменения..."
  git stash pop || echo "Восстановить нечего (stash был пуст). Продолжаем..."

  echo "Переходим в папку сервера"
  cd ./server

  echo "Заменяем порт 5000 на ${NEW_PORT} в docker-compose.yml..."
  sed -i 's/5000:5000/${NEW_PORT}:5000/g' docker-compose.yml

  echo "Останавливаем старые контейнеры..."
  docker-compose down

  echo "Запускаем контейнеры заново на порту ${NEW_PORT}..."
  docker-compose up -d

  echo "Проверяем статус контейнеров..."
  docker-compose ps

EOF

echo "Скрипт deploy_server.sh выполнен."