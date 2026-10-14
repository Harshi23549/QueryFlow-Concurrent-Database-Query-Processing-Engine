USE queryflow;

-- Optional one-time workload expansion for performance measurements.
-- Adds 500 customers, 500 products, 5,000 orders, and 10,000 order lines.
-- Run after seed.sql. Each run creates an additional batch.
SET SESSION cte_max_recursion_depth = 10001;
SET @base_customer = (SELECT COALESCE(MAX(customer_id), 0) + 1 FROM customers);
SET @base_product = (SELECT COALESCE(MAX(product_id), 0) + 1 FROM products);
SET @base_order = (SELECT COALESCE(MAX(order_id), 0) + 1 FROM orders);

INSERT INTO customers (full_name, email)
WITH RECURSIVE seq(n) AS (
  SELECT 1 UNION ALL SELECT n + 1 FROM seq WHERE n < 500
)
SELECT CONCAT('Benchmark Customer ', LPAD(n, 4, '0')),
       CONCAT('benchmark', LPAD(n, 4, '0'), '@example.test')
FROM seq;

INSERT INTO products (product_name, category, unit_price, stock_quantity)
WITH RECURSIVE seq(n) AS (
  SELECT 1 UNION ALL SELECT n + 1 FROM seq WHERE n < 500
)
SELECT CONCAT('Benchmark Product ', LPAD(n, 4, '0')),
       ELT(1 + MOD(n, 8), 'Accessories', 'Displays', 'Office', 'Audio',
           'Storage', 'Networking', 'Components', 'Other'),
       ROUND(50 + MOD(n * 177, 20000) / 100, 2),
       MOD(n * 13, 250)
FROM seq;

INSERT INTO orders (customer_id, order_status, order_date)
WITH RECURSIVE seq(n) AS (
  SELECT 1 UNION ALL SELECT n + 1 FROM seq WHERE n < 5000
)
SELECT @base_customer + MOD(n - 1, 500),
       CASE MOD(n, 10)
         WHEN 0 THEN 'cancelled'
         WHEN 1 THEN 'pending'
         WHEN 2 THEN 'processing'
         ELSE 'completed'
       END,
       DATE_ADD('2026-01-01 00:00:00', INTERVAL MOD(n * 37, 365) DAY)
FROM seq;

INSERT INTO order_items (order_id, product_id, quantity, unit_price)
WITH RECURSIVE seq(n) AS (
  SELECT 1 UNION ALL SELECT n + 1 FROM seq WHERE n < 10000
)
SELECT @base_order + FLOOR((n - 1) / 2),
       p.product_id,
       1 + MOD(n, 4),
       p.unit_price
FROM seq
JOIN products AS p ON p.product_id = @base_product + MOD(n * 37, 500);

SELECT COUNT(*) AS customer_count FROM customers;
SELECT COUNT(*) AS product_count FROM products;
SELECT COUNT(*) AS order_count FROM orders;
SELECT COUNT(*) AS order_line_count FROM order_items;
