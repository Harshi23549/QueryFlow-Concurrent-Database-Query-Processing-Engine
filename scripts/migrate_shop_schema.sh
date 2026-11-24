#!/usr/bin/env bash
set -euo pipefail

DB_DATA_DIR="${QUERYFLOW_DATA_DIR:-/tmp/queryflow-db-data}"
DB_SOCKET="${DB_DATA_DIR}/mysql.sock"
DB_NAME="${QUERYFLOW_DB_NAME:-queryflow}"

mysql_query() {
    mysql --no-defaults --batch --skip-column-names -u root \
        --socket="${DB_SOCKET}" "${DB_NAME}" -e "$1"
}

has_column() {
    local table="$1"
    local column="$2"
    local present
    present="$(mysql_query \
        "SELECT COUNT(*) FROM information_schema.columns WHERE table_schema = DATABASE() AND table_name = '${table}' AND column_name = '${column}'")"
    [ "${present}" != "0" ]
}

ensure_column() {
    local table="$1"
    local column="$2"
    local definition="$3"
    local present
    present="$(mysql_query \
        "SELECT COUNT(*) FROM information_schema.columns WHERE table_schema = DATABASE() AND table_name = '${table}' AND column_name = '${column}'")"
    if [ "${present}" = "0" ]; then
        mysql_query "ALTER TABLE \`${table}\` ADD COLUMN \`${column}\` ${definition}"
    fi
}

ensure_index() {
    local table="$1"
    local index="$2"
    local definition="$3"
    local present
    present="$(mysql_query \
        "SELECT COUNT(*) FROM information_schema.statistics WHERE table_schema = DATABASE() AND table_name = '${table}' AND index_name = '${index}'")"
    if [ "${present}" = "0" ]; then
        mysql_query "ALTER TABLE \`${table}\` ADD ${definition}"
    fi
}

ensure_foreign_key() {
    local table="$1"
    local constraint="$2"
    local definition="$3"
    local present
    present="$(mysql_query \
        "SELECT COUNT(*) FROM information_schema.table_constraints WHERE constraint_schema = DATABASE() AND table_name = '${table}' AND constraint_name = '${constraint}' AND constraint_type = 'FOREIGN KEY'")"
    if [ "${present}" = "0" ]; then
        mysql_query "ALTER TABLE \`${table}\` ADD CONSTRAINT \`${constraint}\` ${definition}"
    fi
}

ensure_column customers password_hash "CHAR(128) NULL"
ensure_column customers password_salt "CHAR(32) NULL"

ensure_column products sku "VARCHAR(40) NULL"
ensure_column products category_id "BIGINT UNSIGNED NULL"
ensure_column products description "VARCHAR(500) NOT NULL DEFAULT ''"
ensure_column products active "BOOLEAN NOT NULL DEFAULT TRUE"
ensure_column products created_at "TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP"

mysql_query "INSERT INTO categories (category_name) VALUES ('Accessories'), ('Displays'), ('Office'), ('Audio'), ('Storage') ON DUPLICATE KEY UPDATE category_name = VALUES(category_name)"
if has_column products category; then
    mysql_query "UPDATE products p LEFT JOIN categories c ON c.category_name = p.category SET p.category_id = COALESCE(c.category_id, (SELECT category_id FROM categories WHERE category_name = 'Accessories' LIMIT 1)) WHERE p.category_id IS NULL"
else
    mysql_query "UPDATE products SET category_id = (SELECT category_id FROM categories WHERE category_name = 'Accessories' LIMIT 1) WHERE category_id IS NULL"
fi
mysql_query "UPDATE products SET sku = CONCAT('LEGACY-', product_id) WHERE sku IS NULL OR sku = ''"
mysql_query "ALTER TABLE products MODIFY sku VARCHAR(40) NOT NULL, MODIFY category_id BIGINT UNSIGNED NOT NULL"
ensure_index products uq_products_sku "UNIQUE KEY \`uq_products_sku\` (\`sku\`)"
ensure_foreign_key products fk_products_category "FOREIGN KEY (\`category_id\`) REFERENCES categories(\`category_id\`) ON UPDATE CASCADE ON DELETE RESTRICT"

ensure_column orders address_id "BIGINT UNSIGNED NULL"
ensure_column orders payment_status "ENUM('simulated','refunded') NOT NULL DEFAULT 'simulated'"
ensure_column orders total_amount "DECIMAL(12,2) NOT NULL DEFAULT 0"
ensure_column orders shipping_address_snapshot "VARCHAR(700) NOT NULL DEFAULT 'Not provided'"
mysql_query "ALTER TABLE orders MODIFY order_status ENUM('pending','processing','completed','cancelled','confirmed','shipped','delivered') NOT NULL DEFAULT 'confirmed'"
mysql_query "UPDATE orders SET order_status = CASE order_status WHEN 'pending' THEN 'confirmed' WHEN 'completed' THEN 'delivered' ELSE order_status END"
mysql_query "ALTER TABLE orders MODIFY order_status ENUM('confirmed','processing','shipped','delivered','cancelled') NOT NULL DEFAULT 'confirmed'"
mysql_query "UPDATE orders o SET total_amount = COALESCE((SELECT SUM(oi.quantity * oi.unit_price) FROM order_items oi WHERE oi.order_id = o.order_id), 0) WHERE total_amount = 0"
ensure_foreign_key orders fk_orders_address "FOREIGN KEY (\`address_id\`) REFERENCES customer_addresses(\`address_id\`) ON UPDATE CASCADE ON DELETE SET NULL"

ensure_column order_items product_name_snapshot "VARCHAR(160) NOT NULL DEFAULT ''"
mysql_query "UPDATE order_items oi JOIN products p ON p.product_id = oi.product_id SET oi.product_name_snapshot = p.product_name WHERE oi.product_name_snapshot = ''"

mysql_query "INSERT INTO order_status_history (order_id, status, note) SELECT o.order_id, o.order_status, 'Imported from existing order history' FROM orders o WHERE NOT EXISTS (SELECT 1 FROM order_status_history h WHERE h.order_id = o.order_id)"
