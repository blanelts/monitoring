#!/bin/bash

set -e  # Останавливаем скрипт при ошибке

# Определяем корневую директорию проекта (где находится скрипт)
BASE_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$BASE_DIR/agent_deb"
TMP_DIR="/tmp/agent_build"
OUT_DIR="$BASE_DIR"
BIN_NAME="agent"

DEB_NAME="agent.deb"
RPM_NAME="agent.rpm"

echo "Очистка временной директории..."
rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR/usr/bin" "$TMP_DIR/usr/lib" "$TMP_DIR/usr/include"

echo "Копирование файлов..."
cp -rp "$SRC_DIR/"* "$TMP_DIR/"

# Копирование необходимых библиотек
echo "Копирование библиотек..."
cp /usr/lib/x86_64-linux-gnu/liblber-2.4.so.2 "$TMP_DIR/usr/lib/"
cp /usr/lib/x86_64-linux-gnu/libldap_r-2.4.so.2 "$TMP_DIR/usr/lib/"
cp /usr/lib/x86_64-linux-gnu/librtmp.so.1 "$TMP_DIR/usr/lib/"
cp /usr/lib/x86_64-linux-gnu/libgssapi.so.3 "$TMP_DIR/usr/lib/"
cp /usr/lib/x86_64-linux-gnu/libsasl2.so.2 "$TMP_DIR/usr/lib/"
cp /usr/lib/x86_64-linux-gnu/libasn1.so.8 "$TMP_DIR/usr/lib/"
cp /usr/lib/x86_64-linux-gnu/libhcrypto.so.4 "$TMP_DIR/usr/lib/"
cp /usr/lib/x86_64-linux-gnu/libheimntlm.so.0 "$TMP_DIR/usr/lib/"
cp /usr/lib/x86_64-linux-gnu/libkrb5.so.26 "$TMP_DIR/usr/lib/"
cp /usr/lib/x86_64-linux-gnu/libroken.so.18 "$TMP_DIR/usr/lib/"
cp /usr/lib/x86_64-linux-gnu/libheimbase.so.1 "$TMP_DIR/usr/lib/"
cp /usr/lib/x86_64-linux-gnu/libhx509.so.5 "$TMP_DIR/usr/lib/"
cp /usr/lib/x86_64-linux-gnu/libwind.so.0 "$TMP_DIR/usr/lib/"

echo "Компиляция бинарного файла..."
g++ -o "$TMP_DIR/usr/bin/$BIN_NAME" "$BASE_DIR/main.cpp" \
    -static -Wl,-Bstatic -lcurl -lidn2 -lpsl -lz -lssl -lcrypto \
    -Wl,-Bdynamic -lgssapi_krb5 -ldl -lrtmp -lldap -llber -lnghttp2 -lunistring -pthread \
    -Wl,--dynamic-linker=/lib64/ld-linux-x86-64.so.2

echo "Изменение прав доступа..."
chmod 755 -R "$TMP_DIR"

### Сборка DEB пакета ###
echo "Сборка .deb пакета..."
dpkg-deb --build "$TMP_DIR"
mv "/tmp/agent_build.deb" "$OUT_DIR/$DEB_NAME"

### Сборка RPM пакета ###
echo "Сборка .rpm пакета..."
RPM_BUILD_DIR="/tmp/rpm_build"
rm -rf "$RPM_BUILD_DIR"
mkdir -p "$RPM_BUILD_DIR/BUILD" "$RPM_BUILD_DIR/RPMS" "$RPM_BUILD_DIR/SOURCES" "$RPM_BUILD_DIR/SPECS" "$RPM_BUILD_DIR/SRPMS"

SPEC_FILE="$RPM_BUILD_DIR/SPECS/agent.spec"

cat <<EOF > "$SPEC_FILE"
Name: agent
Version: 1.0
Release: 1%{?dist}
Summary: Monitoring Agent
License: GPL
Group: System/Monitoring
BuildArch: x86_64
Requires: curl
%description
A monitoring agent that collects system metrics.

%prep

%build

%install
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/usr/lib
mkdir -p %{buildroot}/usr/lib/systemd/system
cp -a $TMP_DIR/usr/bin/$BIN_NAME %{buildroot}/usr/bin/
cp -a $TMP_DIR/usr/lib/* %{buildroot}/usr/lib/
cp -a $TMP_DIR/usr/lib/systemd/system/agent.service %{buildroot}/usr/lib/systemd/system/

%post
#!/bin/bash
set -e

# Используем переданные переменные окружения или задаём значения по умолчанию
SERVER_NAME=${SERVER_NAME:-"default_server"}
INTERFACE=${INTERFACE:-"eth0"}
MOUNT=${MOUNT:-"/"}
URL=${URL:-"http://localhost:5000/report"}
TIME=${TIME:-"60"}

echo "SERVER_NAME=\"$SERVER_NAME\"" > /etc/monitoring_agent.conf
echo "INTERFACE=\"$INTERFACE\"" >> /etc/monitoring_agent.conf
echo "MOUNT=\"$MOUNT\"" >> /etc/monitoring_agent.conf
echo "URL=\"$URL\"" >> /etc/monitoring_agent.conf
echo "TIME=\"$TIME\"" >> /etc/monitoring_agent.conf

chmod 644 /etc/monitoring_agent.conf

systemctl daemon-reload
systemctl enable agent.service
systemctl restart agent.service


%files
/usr/bin/agent
/usr/lib/liblber-2.4.so.2
/usr/lib/libldap_r-2.4.so.2
/usr/lib/librtmp.so.1
/usr/lib/libgssapi.so.3
/usr/lib/libsasl2.so.2
/usr/lib/libasn1.so.8
/usr/lib/libhcrypto.so.4
/usr/lib/libheimntlm.so.0
/usr/lib/libkrb5.so.26
/usr/lib/libroken.so.18
/usr/lib/libheimbase.so.1
/usr/lib/libhx509.so.5
/usr/lib/libwind.so.0
/usr/lib/libcurl.a
/usr/lib/libjsoncpp.a
/usr/lib/systemd/system/agent.service

%changelog
EOF

rpmbuild --define "_topdir $RPM_BUILD_DIR" -bb "$SPEC_FILE"
mv "$RPM_BUILD_DIR/RPMS/x86_64/agent-1.0-1.x86_64.rpm" "$OUT_DIR/$RPM_NAME"

echo "Сборка завершена!"
echo "DEB: $OUT_DIR/$DEB_NAME"
echo "RPM: $OUT_DIR/$RPM_NAME"