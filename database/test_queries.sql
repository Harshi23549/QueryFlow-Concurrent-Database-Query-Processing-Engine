USE queryflow;

-- A. Point read and basic sort
SELECT product_id, product_name, category, unit_price
FROM products
WHERE product_id = 1;

-- B. Filter/range query for the composite-index experiment
SELECT product_id, product_name, unit_price
FROM products
WHERE category = 'Accessories' AND unit_price BETWEEN 1000 AND 4000
ORDER BY unit_price;

-- C. Order detail join with calculated line totals
SELECT o.order_id, o.order_date, c.full_name, p.product_name,
       i.quantity, i.unit_price,
       i.quantity * i.unit_price AS line_total
FROM orders AS o
JOIN customers AS c ON c.customer_id = o.customer_id
JOIN order_items AS i ON i.order_id = o.order_id
JOIN products AS p ON p.product_id = i.product_id
ORDER BY o.order_id, p.product_name;

-- D. Total amount per order
SELECT o.order_id, c.full_name, o.order_status,
       SUM(i.quantity * i.unit_price) AS order_total
FROM orders AS o
JOIN customers AS c ON c.customer_id = o.customer_id
JOIN order_items AS i ON i.order_id = o.order_id
GROUP BY o.order_id, c.full_name, o.order_status
ORDER BY o.order_id;

-- E. Customers with more than one order
SELECT c.customer_id, c.full_name, COUNT(*) AS order_count
FROM customers AS c
JOIN orders AS o ON o.customer_id = c.customer_id
GROUP BY c.customer_id, c.full_name
HAVING COUNT(*) > 1
ORDER BY order_count DESC, c.customer_id;

-- F. Revenue by product category (cancelled orders excluded)
SELECT p.category,
       SUM(i.quantity) AS units_sold,
       SUM(i.quantity * i.unit_price) AS revenue
FROM orders AS o
JOIN order_items AS i ON i.order_id = o.order_id
JOIN products AS p ON p.product_id = i.product_id
WHERE o.order_status <> 'cancelled'
GROUP BY p.category
ORDER BY revenue DESC;

-- G. Monthly revenue by status
SELECT DATE_FORMAT(o.order_date, '%Y-%m') AS order_month,
       o.order_status,
       SUM(i.quantity * i.unit_price) AS gross_total
FROM orders AS o
JOIN order_items AS i ON i.order_id = o.order_id
GROUP BY DATE_FORMAT(o.order_date, '%Y-%m'), o.order_status
ORDER BY order_month, o.order_status;

-- H. Stock lookup and a window function (MySQL 8)
SELECT product_id, product_name, category, stock_quantity,
       RANK() OVER (PARTITION BY category ORDER BY stock_quantity DESC) AS stock_rank
FROM products;

-- I. Read the engine's stored lifecycle and timing metrics
SELECT query_id, worker_id, scheduling_method, query_status,
       wait_time_us, execution_time_us, response_time_us
FROM query_execution_metrics
ORDER BY submitted_at DESC;

-- J. Query-plan experiment. Run once before indexes.sql, then again after it.
EXPLAIN ANALYZE
SELECT product_id, product_name, unit_price
FROM products
WHERE category = 'Accessories' AND unit_price BETWEEN 1000 AND 4000
ORDER BY unit_price;

-- K. Transaction rollback check: final stock must equal the starting stock.
SELECT stock_quantity AS stock_before FROM products WHERE product_id = 1;
START TRANSACTION;
UPDATE products SET stock_quantity = stock_quantity - 1
WHERE product_id = 1 AND stock_quantity > 0;
SELECT stock_quantity AS stock_inside_transaction FROM products WHERE product_id = 1;
ROLLBACK;
SELECT stock_quantity AS stock_after_rollback FROM products WHERE product_id = 1;

-- L. Constraint check (uncomment individually to confirm rejection).
-- INSERT INTO order_items(order_id, product_id, quantity, unit_price) VALUES (1, 1, 0, 0);
-- INSERT INTO orders(customer_id, order_status) VALUES (999999, 'pending');

-- Error-path check for connector error handling (uncomment intentionally).
-- SELECT * FROM table_that_does_not_exist;
