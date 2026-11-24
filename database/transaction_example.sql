-- Run against the initialized QueryFlow schema after applying seed.sql.
START TRANSACTION;
INSERT INTO customers (full_name, email)
VALUES ('Transaction Example', 'transaction-example@queryflow.local');
SET @customer_id = LAST_INSERT_ID();
INSERT INTO customer_addresses
  (customer_id, recipient_name, address_line, city, region, postal_code, country)
VALUES
  (@customer_id, 'Transaction Example', '1 Example Street', 'Example City',
   'Example Region', '00000', 'India');
SET @address_id = LAST_INSERT_ID();
SELECT product_id, unit_price INTO @product_id, @unit_price
FROM products WHERE sku = 'ACC-KEY-001' FOR UPDATE;
SELECT stock_quantity INTO @stock_before
FROM products WHERE product_id = @product_id;
INSERT INTO orders
  (customer_id, address_id, order_status, payment_status, total_amount,
   shipping_address_snapshot)
VALUES
  (@customer_id, @address_id, 'confirmed', 'simulated', @unit_price,
   'Transaction Example, 1 Example Street, Example City, Example Region 00000, India');
SET @order_id = LAST_INSERT_ID();
INSERT INTO order_items
  (order_id, product_id, product_name_snapshot, quantity, unit_price)
SELECT @order_id, product_id, product_name, 1, unit_price
FROM products WHERE product_id = @product_id;
UPDATE products SET stock_quantity = stock_quantity - 1
WHERE product_id = @product_id AND stock_quantity > 0;
INSERT INTO payments (order_id, payment_method, payment_status, amount)
VALUES (@order_id, 'classroom_simulation', 'simulated', @unit_price);
INSERT INTO order_status_history (order_id, status, note)
VALUES (@order_id, 'confirmed', 'Transaction example; rolled back');
SELECT @stock_before AS stock_before, stock_quantity AS stock_during_example
FROM products WHERE product_id = @product_id;
ROLLBACK;
