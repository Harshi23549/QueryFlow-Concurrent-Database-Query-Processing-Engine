USE queryflow;

-- Lifecycle logging walkthrough; all changes are rolled back at the end.
START TRANSACTION;
INSERT INTO query_execution_log (scheduling_method, priority, query_label)
VALUES ('FCFS', 0, 'transaction_demo');
SET @demo_query_id = LAST_INSERT_ID();
UPDATE query_execution_log
SET query_status = 'running', worker_id = 'worker-demo', started_at = CURRENT_TIMESTAMP(6)
WHERE query_id = @demo_query_id;
UPDATE query_execution_log
SET query_status = 'succeeded', finished_at = CURRENT_TIMESTAMP(6), affected_rows = 1
WHERE query_id = @demo_query_id;
SELECT query_id, query_status, wait_time_us, execution_time_us, response_time_us
FROM query_execution_metrics WHERE query_id = @demo_query_id;
ROLLBACK;

-- Transactional inventory/order example; also rolled back to keep seed data unchanged.
START TRANSACTION;
SELECT stock_quantity INTO @stock_before FROM products WHERE product_id = 1 FOR UPDATE;
INSERT INTO orders (customer_id, order_status) VALUES (1, 'pending');
SET @demo_order_id = LAST_INSERT_ID();
INSERT INTO order_items (order_id, product_id, quantity, unit_price)
SELECT @demo_order_id, product_id, 1, unit_price FROM products WHERE product_id = 1;
UPDATE products SET stock_quantity = stock_quantity - 1
WHERE product_id = 1 AND stock_quantity > 0;
SELECT @stock_before AS stock_before, stock_quantity AS stock_during_demo
FROM products WHERE product_id = 1;
ROLLBACK;
