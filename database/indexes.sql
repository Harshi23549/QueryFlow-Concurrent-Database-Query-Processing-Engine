USE queryflow;

-- Baseline indexes are declared in schema.sql for primary/foreign-key and common
-- lifecycle lookups. Record the baseline EXPLAIN ANALYZE output first.
-- Apply this optional index once, then rerun section J of test_queries.sql.
CREATE INDEX idx_products_category_price
  ON products (category, unit_price);

-- Compare actual plan, rows examined, and elapsed time with the baseline.
-- Drop the experiment index to restore baseline:
-- DROP INDEX idx_products_category_price ON products;
