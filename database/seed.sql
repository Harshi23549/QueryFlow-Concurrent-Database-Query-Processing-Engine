USE queryflow;

-- This seed is intended for a fresh schema. It uses fixed rows so workload
-- examples have predictable values and IDs. Run once after schema.sql.
START TRANSACTION;
INSERT INTO customers (full_name, email) VALUES
  ('Aarav Mehta', 'aarav@example.test'),
  ('Diya Sharma', 'diya@example.test'),
  ('Kabir Rao', 'kabir@example.test'),
  ('Meera Nair', 'meera@example.test'),
  ('Ishaan Gupta', 'ishaan@example.test'),
  ('Anaya Iyer', 'anaya@example.test'),
  ('Rehan Khan', 'rehan@example.test'),
  ('Sara Das', 'sara@example.test');

INSERT INTO products (product_name, category, unit_price, stock_quantity) VALUES
  ('Mechanical Keyboard', 'Accessories', 3499.00, 25),
  ('Wireless Mouse', 'Accessories', 1299.00, 40),
  ('USB-C Hub', 'Accessories', 2199.00, 18),
  ('27-inch Monitor', 'Displays', 18499.00, 12),
  ('Laptop Stand', 'Office', 1599.00, 30),
  ('Webcam', 'Accessories', 4299.00, 15),
  ('Noise-Cancelling Headset', 'Audio', 7499.00, 20),
  ('Desk Lamp', 'Office', 1899.00, 22),
  ('Portable SSD 1TB', 'Storage', 8999.00, 14),
  ('1080p Projector', 'Displays', 28999.00, 5),
  ('Bluetooth Speaker', 'Audio', 3299.00, 17),
  ('Wireless Charger', 'Accessories', 999.00, 35);

INSERT INTO orders (customer_id, order_status, order_date) VALUES
  (1, 'completed',  '2026-09-10 10:15:00'),
  (2, 'processing', '2026-09-11 12:30:00'),
  (1, 'completed',  '2026-09-12 09:00:00'),
  (3, 'pending',    '2026-09-13 14:45:00'),
  (4, 'completed',  '2026-09-14 16:20:00'),
  (5, 'cancelled',  '2026-09-15 11:05:00'),
  (6, 'completed',  '2026-09-16 13:10:00'),
  (7, 'processing', '2026-09-17 17:40:00'),
  (8, 'completed',  '2026-09-18 08:25:00'),
  (2, 'completed',  '2026-09-19 15:00:00'),
  (3, 'pending',    '2026-09-20 10:55:00'),
  (5, 'completed',  '2026-09-21 18:30:00');

INSERT INTO order_items (order_id, product_id, quantity, unit_price) VALUES
  (1, 1, 1, 3499.00), (1, 2, 1, 1299.00),
  (2, 4, 1, 18499.00), (2, 3, 1, 2199.00),
  (3, 5, 2, 1599.00), (3, 6, 1, 4299.00),
  (4, 2, 2, 1299.00), (4, 12, 1, 999.00),
  (5, 4, 1, 18499.00), (5, 7, 1, 7499.00),
  (6, 8, 2, 1899.00),
  (7, 9, 1, 8999.00), (7, 2, 1, 1299.00),
  (8, 10, 1, 28999.00), (8, 11, 2, 3299.00),
  (9, 1, 1, 3499.00), (9, 5, 1, 1599.00),
  (10, 3, 2, 2199.00), (10, 12, 2, 999.00),
  (11, 6, 1, 4299.00), (11, 7, 1, 7499.00),
  (12, 4, 1, 18499.00), (12, 9, 1, 8999.00);
COMMIT;
