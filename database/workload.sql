-- Sample read-only workload for the QueryFlow batch runner.
-- These statements use tables created by schema.sql and seeded by seed.sql.

-- Retrieve a few customers.
SELECT customer_id, full_name, email
FROM customers
ORDER BY customer_id
LIMIT 3;

-- Count orders.
SELECT COUNT(*) AS order_count
FROM orders;

-- Retrieve customer names.
SELECT customer_id, full_name
FROM customers
ORDER BY customer_id
LIMIT 5;

-- Count customers.
SELECT COUNT(*) AS customer_count
FROM customers;

-- Retrieve recent orders.
SELECT order_id, customer_id, order_status, order_date
FROM orders
ORDER BY order_date DESC
LIMIT 5;
