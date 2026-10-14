CREATE DATABASE IF NOT EXISTS queryflow
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_0900_ai_ci;
USE queryflow;

-- Small business dataset for joins, aggregates, read/write SQL, and indexing labs.
CREATE TABLE IF NOT EXISTS customers (
  customer_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  full_name VARCHAR(120) NOT NULL,
  email VARCHAR(254) NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (customer_id),
  UNIQUE KEY uq_customers_email (email)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS products (
  product_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  product_name VARCHAR(160) NOT NULL,
  category VARCHAR(80) NOT NULL,
  unit_price DECIMAL(10,2) NOT NULL,
  stock_quantity INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (product_id),
  KEY idx_products_name (product_name),
  CONSTRAINT chk_products_price CHECK (unit_price >= 0)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS orders (
  order_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  customer_id BIGINT UNSIGNED NOT NULL,
  order_status ENUM('pending','processing','completed','cancelled') NOT NULL DEFAULT 'pending',
  order_date DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (order_id),
  KEY idx_orders_customer_date (customer_id, order_date),
  KEY idx_orders_status_date (order_status, order_date),
  CONSTRAINT fk_orders_customer FOREIGN KEY (customer_id)
    REFERENCES customers(customer_id) ON UPDATE CASCADE ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS order_items (
  order_item_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  order_id BIGINT UNSIGNED NOT NULL,
  product_id BIGINT UNSIGNED NOT NULL,
  quantity INT UNSIGNED NOT NULL,
  unit_price DECIMAL(10,2) NOT NULL,
  PRIMARY KEY (order_item_id),
  UNIQUE KEY uq_order_product (order_id, product_id),
  KEY idx_order_items_product (product_id),
  CONSTRAINT chk_order_items_quantity CHECK (quantity > 0),
  CONSTRAINT chk_order_items_price CHECK (unit_price >= 0),
  CONSTRAINT fk_items_order FOREIGN KEY (order_id)
    REFERENCES orders(order_id) ON UPDATE CASCADE ON DELETE CASCADE,
  CONSTRAINT fk_items_product FOREIGN KEY (product_id)
    REFERENCES products(product_id) ON UPDATE CASCADE ON DELETE RESTRICT
) ENGINE=InnoDB;

-- Optional durable lifecycle history for the query manager/scheduler.
-- Store a safe label and error summary, not raw SQL or bound parameter values.
CREATE TABLE IF NOT EXISTS query_execution_log (
  query_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  worker_id VARCHAR(64) NULL,
  scheduling_method ENUM('FCFS','PRIORITY') NOT NULL,
  priority INT NOT NULL DEFAULT 0,
  query_label VARCHAR(120) NOT NULL,
  query_status ENUM('queued','running','succeeded','failed','cancelled') NOT NULL DEFAULT 'queued',
  submitted_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  started_at DATETIME(6) NULL,
  finished_at DATETIME(6) NULL,
  affected_rows BIGINT UNSIGNED NULL,
  error_code VARCHAR(64) NULL,
  error_message TEXT NULL,
  PRIMARY KEY (query_id),
  KEY idx_query_status_submitted (query_status, submitted_at),
  KEY idx_query_worker_started (worker_id, started_at),
  CONSTRAINT chk_query_start_after_submit
    CHECK (started_at IS NULL OR started_at >= submitted_at),
  CONSTRAINT chk_query_finish_after_start
    CHECK (finished_at IS NULL OR (started_at IS NOT NULL AND finished_at >= started_at)),
  CONSTRAINT chk_query_completion_data
    CHECK (query_status NOT IN ('succeeded','failed','cancelled') OR finished_at IS NOT NULL)
) ENGINE=InnoDB;

-- Times are in microseconds and remain NULL until the relevant lifecycle time exists.
CREATE OR REPLACE VIEW query_execution_metrics AS
SELECT query_id, worker_id, scheduling_method, priority, query_label, query_status,
       submitted_at, started_at, finished_at, affected_rows, error_code,
       CASE WHEN started_at IS NULL THEN NULL
            ELSE TIMESTAMPDIFF(MICROSECOND, submitted_at, started_at) END AS wait_time_us,
       CASE WHEN started_at IS NULL OR finished_at IS NULL THEN NULL
            ELSE TIMESTAMPDIFF(MICROSECOND, started_at, finished_at) END AS execution_time_us,
       CASE WHEN finished_at IS NULL THEN NULL
            ELSE TIMESTAMPDIFF(MICROSECOND, submitted_at, finished_at) END AS response_time_us
FROM query_execution_log;
