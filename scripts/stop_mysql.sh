#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

DB_DATA_DIR="${QUERYFLOW_DATA_DIR:-/tmp/queryflow-db-data}"
DB_PID_FILE="${DB_DATA_DIR}/mysqld.pid"


if [ ! -f "${DB_PID_FILE}" ]; then
    echo "MySQL is not running (no PID file found)."
    exit 0
fi

PID="$(cat "${DB_PID_FILE}")"
if ! kill -0 "${PID}" 2>/dev/null; then
    echo "MySQL process ${PID} is not running. Removing stale PID file."
    rm -f "${DB_PID_FILE}"
    exit 0
fi

echo "Stopping MySQL daemon (PID: ${PID})..."
kill "${PID}"

for i in {1..30}; do
    if ! kill -0 "${PID}" 2>/dev/null; then
        break
    fi
    sleep 0.5
done

if kill -0 "${PID}" 2>/dev/null; then
    echo "Forcing shutdown..."
    kill -9 "${PID}" 2>/dev/null || true
fi

rm -f "${DB_PID_FILE}"
echo "MySQL stopped successfully."
