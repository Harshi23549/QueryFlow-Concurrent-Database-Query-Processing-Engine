BUILD_DIR ?= build
BUILD_MYSQL_DIR ?= build-mysql
BUILD_TYPE ?= Debug
STOREFRONT_HOST ?= 0.0.0.0
STOREFRONT_PORT ?= 8080
ADMIN_PORT ?= 8081

.PHONY: all configure build-mysql configure-mysql run project run-mysql test test-mysql db-start db-stop db-setup db-transaction-example clean

all: configure
	cmake --build $(BUILD_DIR)

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build-mysql: configure-mysql
	cmake --build $(BUILD_MYSQL_DIR)

configure-mysql:
	cmake -S . -B $(BUILD_MYSQL_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DQUERYFLOW_ENABLE_MYSQL=ON

run: all
	./$(BUILD_DIR)/QueryFlow

project: db-setup
	cmake -S . -B build-project -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DQUERYFLOW_ENABLE_MYSQL=ON
	cmake --build build-project
	./build-project/QueryFlow --backend mysql --web --host $(STOREFRONT_HOST) --port $(STOREFRONT_PORT) --admin-port $(ADMIN_PORT) --workers 4 --policy priority

run-mysql: build-mysql
	./$(BUILD_MYSQL_DIR)/QueryFlow --backend mysql

test: all
	ctest --test-dir $(BUILD_DIR) --output-on-failure

test-mysql: build-mysql
	ctest --test-dir $(BUILD_MYSQL_DIR) --output-on-failure

db-start:
	./scripts/start_mysql.sh

db-stop:
	./scripts/stop_mysql.sh

db-setup:
	./scripts/setup_mysql.sh

db-transaction-example:
	cmake -S database -B database/build -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	cmake --build database/build
	./database/build/queryflow_db_transaction_example

clean:
	cmake -E rm -rf $(BUILD_DIR) $(BUILD_MYSQL_DIR) build-project database/build
