#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

DB_DATA_DIR="${QUERYFLOW_DATA_DIR:-/tmp/queryflow-db-data}"
DB_PID_FILE="${DB_DATA_DIR}/mysqld.pid"
DB_SOCKET="${DB_DATA_DIR}/mysql.sock"
DB_LOG="${DB_DATA_DIR}/mysqld.log"
DB_PORT="${QUERYFLOW_DB_PORT:-3307}"
DB_USER="${QUERYFLOW_DB_USER:-queryflow_app}"
DB_PASS="${QUERYFLOW_DB_PASSWORD:-queryflow_pass}"
DB_NAME="${QUERYFLOW_DB_NAME:-queryflow}"


# Find mysqld
MYSQLD=""
if [ -x "/usr/sbin/mysqld" ]; then
    MYSQLD="/usr/sbin/mysqld"
elif command -v mysqld >/dev/null 2>&1; then
    MYSQLD="$(command -v mysqld)"
fi

if [ -z "${MYSQLD}" ]; then
    echo "ERROR: mysqld binary not found." >&2
    exit 1
fi

# Initialize data directory if not present
if [ ! -d "${DB_DATA_DIR}/mysql" ]; then
    echo "==> Initializing MySQL data directory at ${DB_DATA_DIR}..."
    if [ -d "${DB_DATA_DIR}" ]; then
        rmdir "${DB_DATA_DIR}" 2>/dev/null || true
    fi
    "${MYSQLD}" --no-defaults --initialize-insecure \
        --datadir="${DB_DATA_DIR}" \
        --log-error="${DB_LOG}"
    echo "==> Initialization complete."
fi


# Start MySQL if not running
if [ -f "${DB_PID_FILE}" ] && kill -0 "$(cat "${DB_PID_FILE}")" 2>/dev/null; then
    echo "==> MySQL is already running (PID: $(cat "${DB_PID_FILE}"))."
else
    echo "==> Starting MySQL daemon on port ${DB_PORT}..."
    "${MYSQLD}" --no-defaults \
        --datadir="${DB_DATA_DIR}" \
        --port="${DB_PORT}" \
        --socket="${DB_SOCKET}" \
        --pid-file="${DB_PID_FILE}" \
        --log-error="${DB_LOG}" \
        --daemonize

    # Wait for socket to be ready
    for i in {1..30}; do
        if [ -S "${DB_SOCKET}" ]; then
            break
        fi
        sleep 0.5
    done
    if [ ! -S "${DB_SOCKET}" ]; then
        echo "ERROR: MySQL failed to start. Check ${DB_LOG} for details." >&2
        exit 1
    fi
    echo "==> MySQL started successfully (PID: $(cat "${DB_PID_FILE}"))."
fi

# Configure users and database
echo "==> Configuring database and credentials..."
mysql -u root --socket="${DB_SOCKET}" <<EOF
CREATE DATABASE IF NOT EXISTS ${DB_NAME} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
CREATE USER IF NOT EXISTS '${DB_USER}'@'%' IDENTIFIED BY '${DB_PASS}';
CREATE USER IF NOT EXISTS '${DB_USER}'@'localhost' IDENTIFIED BY '${DB_PASS}';
CREATE USER IF NOT EXISTS '${DB_USER}'@'127.0.0.1' IDENTIFIED BY '${DB_PASS}';
GRANT ALL PRIVILEGES ON ${DB_NAME}.* TO '${DB_USER}'@'%';
GRANT ALL PRIVILEGES ON ${DB_NAME}.* TO '${DB_USER}'@'localhost';
GRANT ALL PRIVILEGES ON ${DB_NAME}.* TO '${DB_USER}'@'127.0.0.1';
FLUSH PRIVILEGES;
EOF

# Apply schema and seed
echo "==> Applying schema..."
mysql -u root --socket="${DB_SOCKET}" "${DB_NAME}" < "${PROJECT_ROOT}/database/schema.sql"

echo "==> Applying seed data..."
mysql -u root --socket="${DB_SOCKET}" "${DB_NAME}" < "${PROJECT_ROOT}/database/seed.sql"

# Generate .env file
cat > "${PROJECT_ROOT}/.env" <<EOF
QUERYFLOW_DB_HOST=127.0.0.1
QUERYFLOW_DB_PORT=${DB_PORT}
QUERYFLOW_DB_NAME=${DB_NAME}
QUERYFLOW_DB_USER=${DB_USER}
QUERYFLOW_DB_PASSWORD=${DB_PASS}
EOF

echo "==> Wrote connection configuration to ${PROJECT_ROOT}/.env"
echo "==> QueryFlow database is ready to use!"
