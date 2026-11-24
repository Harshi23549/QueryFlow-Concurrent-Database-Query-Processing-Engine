INSERT INTO categories (category_name) VALUES
  ('Accessories'), ('Displays'), ('Office'), ('Audio'), ('Storage')
ON DUPLICATE KEY UPDATE category_name = VALUES(category_name);

INSERT INTO products
  (sku, category_id, product_name, description, unit_price, stock_quantity)
SELECT 'ACC-KEY-001', category_id, 'Mechanical Keyboard',
       'Compact mechanical keyboard with tactile switches.', 3499.00, 25
FROM categories WHERE category_name = 'Accessories'
ON DUPLICATE KEY UPDATE product_name = VALUES(product_name),
  description = VALUES(description), unit_price = VALUES(unit_price), active = TRUE;

INSERT INTO products
  (sku, category_id, product_name, description, unit_price, stock_quantity)
SELECT 'ACC-MOU-001', category_id, 'Wireless Mouse',
       'Ergonomic wireless mouse with adjustable sensitivity.', 1299.00, 40
FROM categories WHERE category_name = 'Accessories'
ON DUPLICATE KEY UPDATE product_name = VALUES(product_name),
  description = VALUES(description), unit_price = VALUES(unit_price), active = TRUE;

INSERT INTO products
  (sku, category_id, product_name, description, unit_price, stock_quantity)
SELECT 'DSP-MON-001', category_id, '27-inch Monitor',
       '27-inch QHD monitor for work and entertainment.', 18499.00, 12
FROM categories WHERE category_name = 'Displays'
ON DUPLICATE KEY UPDATE product_name = VALUES(product_name),
  description = VALUES(description), unit_price = VALUES(unit_price), active = TRUE;

INSERT INTO products
  (sku, category_id, product_name, description, unit_price, stock_quantity)
SELECT 'OFF-STD-001', category_id, 'Laptop Stand',
       'Adjustable aluminum laptop stand.', 1599.00, 30
FROM categories WHERE category_name = 'Office'
ON DUPLICATE KEY UPDATE product_name = VALUES(product_name),
  description = VALUES(description), unit_price = VALUES(unit_price), active = TRUE;

INSERT INTO products
  (sku, category_id, product_name, description, unit_price, stock_quantity)
SELECT 'AUD-HST-001', category_id, 'Noise-Cancelling Headset',
       'Over-ear headset with active noise reduction.', 7499.00, 20
FROM categories WHERE category_name = 'Audio'
ON DUPLICATE KEY UPDATE product_name = VALUES(product_name),
  description = VALUES(description), unit_price = VALUES(unit_price), active = TRUE;

INSERT INTO products
  (sku, category_id, product_name, description, unit_price, stock_quantity)
SELECT 'STO-SSD-001', category_id, 'Portable SSD 1TB',
       'Portable solid-state drive with 1 TB capacity.', 8999.00, 14
FROM categories WHERE category_name = 'Storage'
ON DUPLICATE KEY UPDATE product_name = VALUES(product_name),
  description = VALUES(description), unit_price = VALUES(unit_price), active = TRUE;

INSERT INTO products
  (sku, category_id, product_name, description, unit_price, stock_quantity)
SELECT 'ACC-HUB-001', category_id, 'USB-C Hub',
       'Multiport USB-C hub for laptops.', 2199.00, 18
FROM categories WHERE category_name = 'Accessories'
ON DUPLICATE KEY UPDATE product_name = VALUES(product_name),
  description = VALUES(description), unit_price = VALUES(unit_price), active = TRUE;

INSERT INTO products
  (sku, category_id, product_name, description, unit_price, stock_quantity)
SELECT 'OFF-LMP-001', category_id, 'Desk Lamp',
       'Dimmable LED desk lamp.', 1899.00, 22
FROM categories WHERE category_name = 'Office'
ON DUPLICATE KEY UPDATE product_name = VALUES(product_name),
  description = VALUES(description), unit_price = VALUES(unit_price), active = TRUE;
