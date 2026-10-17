#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

DB_DATA_DIR="${QUERYFLOW_DATA_DIR:-/tmp/queryflow-db-data}"
DB_PID_FILE="${DB_DATA_DIR}/mysqld.pid"
DB_SOCKET="${DB_DATA_DIR}/mysql.sock"
DB_LOG="${DB_DATA_DIR}/mysqld.log"
DB_PORT="${QUERYFLOW_DB_PORT:-3307}"


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

if [ ! -d "${DB_DATA_DIR}/mysql" ]; then
    echo "Database has not been initialized. Running setup_mysql.sh first..."
    exec "${SCRIPT_DIR}/setup_mysql.sh"
fi

if [ -f "${DB_PID_FILE}" ] && kill -0 "$(cat "${DB_PID_FILE}")" 2>/dev/null; then
    echo "MySQL is already running (PID: $(cat "${DB_PID_FILE}"))."
    exit 0
fi

echo "Starting MySQL daemon on port ${DB_PORT}..."
"${MYSQLD}" --no-defaults \
    --datadir="${DB_DATA_DIR}" \
    --port="${DB_PORT}" \
    --socket="${DB_SOCKET}" \
    --pid-file="${DB_PID_FILE}" \
    --log-error="${DB_LOG}" \
    --daemonize

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

echo "MySQL started successfully (PID: $(cat "${DB_PID_FILE}"))."
