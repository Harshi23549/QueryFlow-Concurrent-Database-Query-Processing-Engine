(() => {
  const tokenKey = "queryflow-customer-session";
  const state = { token: localStorage.getItem(tokenKey), customer: null, products: [], cart: [] };
  const connection = document.querySelector("#connectionStatus");
  const feedback = document.querySelector("#authFeedback");

  function escapeHtml(value) {
    return String(value ?? "").replace(/[&<>"']/g, (character) => ({
      "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
    })[character]);
  }

  function money(value) {
    return new Intl.NumberFormat("en-IN", {
      style: "currency", currency: "INR", minimumFractionDigits: 2
    }).format(Number(value) || 0);
  }

  async function api(path, options = {}) {
    const headers = { "Content-Type": "application/json", ...(options.headers || {}) };
    if (state.token) headers["X-QueryFlow-Session"] = state.token;
    const response = await fetch(path, { ...options, headers });
    const body = await response.json();
    if (!response.ok) throw new Error(body.error || `Request failed (${response.status})`);
    return body;
  }

  function setFeedback(message, isError = false) {
    feedback.textContent = message;
    feedback.classList.toggle("error", isError);
  }

  function setCustomer(customer, token) {
    state.customer = customer;
    state.token = token;
    if (token) localStorage.setItem(tokenKey, token);
    document.querySelector("#signedOut").hidden = Boolean(customer);
    document.querySelector("#signedIn").hidden = !customer;
    document.querySelector("#customerName").textContent = customer?.fullName || "";
    document.querySelector("#customerEmail").textContent = customer?.email || "";
    document.querySelector("#cartStatus").textContent = customer
      ? "Saved to your account" : "Sign in to save your cart";
    if (!customer) {
      state.cart = [];
      renderCart();
      document.querySelector("#orderHistory").innerHTML =
        '<div class="empty-state">Sign in to see your orders.</div>';
    }
    renderProducts();
  }

  async function loadProducts() {
    state.products = await api("/api/products");
    const categories = [...new Set(state.products.map((item) => item.category_name))];
    document.querySelector("#categoryFilter").innerHTML =
      '<option value="all">All products</option>' +
      categories.map((item) =>
        `<option value="${escapeHtml(item)}">${escapeHtml(item)}</option>`).join("");
    renderProducts();
  }

  function renderProducts() {
    const selected = document.querySelector("#categoryFilter").value;
    const products = state.products.filter((item) =>
      selected === "all" || item.category_name === selected);
    const grid = document.querySelector("#productGrid");
    if (!products.length) {
      grid.innerHTML = '<div class="empty-state">No products in this category.</div>';
      return;
    }
    grid.innerHTML = products.map((product, index) => {
      const available = Number(product.stock_quantity) > 0;
      return `<article class="product-card">
        <div class="product-visual product-visual-${index % 4}">
          <span>${escapeHtml(product.category_name)}</span><b>${escapeHtml(product.product_name)}</b>
          <small>${escapeHtml(product.sku)}</small>
        </div>
        <div class="product-info">
          <div class="product-title"><h3>${escapeHtml(product.product_name)}</h3><strong>${money(product.unit_price)}</strong></div>
          <p>${escapeHtml(product.description)}</p>
          <div class="product-actions"><span class="stock-note">${available ? `${escapeHtml(product.stock_quantity)} in stock` : "Out of stock"}</span>
            <button class="button button-secondary add-to-cart" data-product-id="${escapeHtml(product.product_id)}" type="button" ${available ? "" : "disabled"}>Add to cart</button></div>
        </div>
      </article>`;
    }).join("");
    grid.querySelectorAll(".add-to-cart").forEach((button) => {
      button.addEventListener("click", () => addToCart(button.dataset.productId));
    });
  }

  async function loadCart() {
    if (!state.customer) return;
    state.cart = await api("/api/cart");
    renderCart();
  }

  function renderCart() {
    const itemCount = state.cart.reduce((sum, item) => sum + Number(item.quantity), 0);
    document.querySelector("#cartCount").textContent = itemCount;
    const container = document.querySelector("#cartItems");
    const total = state.cart.reduce((sum, item) =>
      sum + Number(item.unit_price) * Number(item.quantity), 0);
    document.querySelector("#cartTotal").textContent = money(total);
    document.querySelector("#checkoutButton").disabled =
      !state.customer || state.cart.length === 0;
    if (!state.customer) {
      container.innerHTML = '<div class="empty-state">Sign in to add products to your saved cart.</div>';
      return;
    }
    if (!state.cart.length) {
      container.innerHTML = '<div class="empty-state">Your basket is empty. Add something from the collection.</div>';
      return;
    }
    container.innerHTML = state.cart.map((item) => `<article class="cart-item">
      <div class="cart-item-mark">QF</div>
      <div class="cart-item-info"><b>${escapeHtml(item.product_name)}</b><span>${money(item.unit_price)} each · quantity ${escapeHtml(item.quantity)}</span></div>
      <strong>${money(Number(item.unit_price) * Number(item.quantity))}</strong>
      <button class="remove-item" type="button" aria-label="Remove ${escapeHtml(item.product_name)}" data-product-id="${escapeHtml(item.product_id)}">Remove</button>
    </article>`).join("");
    container.querySelectorAll(".remove-item").forEach((button) => {
      button.addEventListener("click", () => removeFromCart(button.dataset.productId));
    });
  }

  async function addToCart(productId) {
    if (!state.customer) {
      document.querySelector("#signedOut").scrollIntoView({ behavior: "smooth" });
      setFeedback("Sign in or create an account before adding items.", true);
      return;
    }
    try {
      await api("/api/cart/items", {
        method: "POST",
        body: JSON.stringify({ productId: Number(productId), quantity: 1 })
      });
      await loadCart();
    } catch (error) {
      setFeedback(error.message, true);
    }
  }

  async function removeFromCart(productId) {
    try {
      await api(`/api/cart/items/${encodeURIComponent(productId)}`, { method: "DELETE" });
      await loadCart();
    } catch (error) {
      setFeedback(error.message, true);
    }
  }

  async function loadOrders() {
    if (!state.customer) return;
    const rows = await api("/api/orders");
    const orders = new Map();
    rows.forEach((row) => {
      if (!orders.has(row.order_id)) {
        orders.set(row.order_id, { ...row, items: [] });
      }
      if (row.product_name_snapshot) orders.get(row.order_id).items.push(row);
    });
    const list = [...orders.values()];
    const container = document.querySelector("#orderHistory");
    if (!list.length) {
      container.innerHTML = '<div class="empty-state">No orders yet. Your confirmed orders will appear here.</div>';
      return;
    }
    container.innerHTML = list.map((order) => `<article class="order-card">
      <div class="order-heading"><div><span class="eyebrow">Order #${escapeHtml(order.order_id)}</span><h3>${escapeHtml(order.order_status)}</h3></div><strong>${money(order.total_amount)}</strong></div>
      <p class="muted small-copy">${escapeHtml(order.order_date)} · Payment ${escapeHtml(order.payment_status)} · ${escapeHtml(order.shipping_address_snapshot)}</p>
      <ul>${order.items.map((item) =>
        `<li>${escapeHtml(item.product_name_snapshot)} × ${escapeHtml(item.quantity)} <span>${money(Number(item.unit_price) * Number(item.quantity))}</span></li>`).join("")}</ul>
    </article>`).join("");
  }

  function showConfirmation(receipt) {
    const confirmation = document.querySelector("#confirmation");
    confirmation.hidden = false;
    confirmation.innerHTML = `<p class="eyebrow">Order confirmed</p>
      <h2>Thank you. Order #${escapeHtml(receipt.orderId)} is in the system.</h2>
      <p>${escapeHtml(receipt.confirmation.message)}</p>
      <div class="confirmation-meta"><span>Status <b>${escapeHtml(receipt.status)}</b></span>
        <span>Payment <b>${escapeHtml(receipt.paymentStatus)}</b></span>
        <span>Total <b>${money(receipt.total)}</b></span>
        <span>Engine query <b>Q${escapeHtml(receipt.confirmation.queryId)}</b></span>
        <span>Worker <b>${escapeHtml(receipt.confirmation.workerId)}</b></span>
        <span>Queue wait <b>${Number(receipt.confirmation.waitingMs).toFixed(1)} ms</b></span>
        <span>Execution <b>${Number(receipt.confirmation.executionMs).toFixed(1)} ms</b></span>
      </div>
      <ul class="receipt-items">${receipt.items.map((item) =>
        `<li>${escapeHtml(item.product_name_snapshot)} × ${escapeHtml(item.quantity)}</li>`).join("")}</ul>`;
      confirmation.scrollIntoView({ behavior: "smooth", block: "center" });
  }

  document.querySelectorAll(".auth-tab").forEach((button) => {
    button.addEventListener("click", () => {
      const register = button.dataset.authMode === "register";
      document.querySelectorAll(".auth-tab").forEach((tab) =>
        tab.classList.toggle("active", tab === button));
      document.querySelector("#loginForm").hidden = register;
      document.querySelector("#registerForm").hidden = !register;
      setFeedback("");
    });
  });

  async function submitAccount(form, path) {
    const values = Object.fromEntries(new FormData(form).entries());
    try {
      const result = await api(path, {
        method: "POST",
        body: JSON.stringify(values)
      });
      setCustomer(result.customer, result.token);
      form.reset();
      setFeedback("");
      await Promise.all([loadCart(), loadOrders()]);
    } catch (error) {
      setFeedback(error.message, true);
    }
  }

  document.querySelector("#loginForm").addEventListener("submit", (event) => {
    event.preventDefault();
    submitAccount(event.currentTarget, "/api/login");
  });
  document.querySelector("#registerForm").addEventListener("submit", (event) => {
    event.preventDefault();
    submitAccount(event.currentTarget, "/api/register");
  });
  document.querySelector("#logoutButton").addEventListener("click", async () => {
    try {
      await api("/api/logout", { method: "POST" });
    } catch (error) {
      setFeedback(`Signed out locally; server session cleanup failed: ${error.message}`, true);
    } finally {
      localStorage.removeItem(tokenKey);
      state.token = null;
      setCustomer(null, null);
    }
  });
  document.querySelector("#categoryFilter").addEventListener("change", renderProducts);
  document.querySelector("#refreshOrdersButton").addEventListener("click", () => {
    loadOrders().catch((error) => setFeedback(error.message, true));
  });
  document.querySelector("#checkoutForm").addEventListener("submit", async (event) => {
    event.preventDefault();
    if (!state.customer || !state.cart.length) return;
    const button = document.querySelector("#checkoutButton");
    button.disabled = true;
    button.textContent = "Processing order…";
    try {
      const address = Object.fromEntries(new FormData(event.currentTarget).entries());
      const receipt = await api("/api/checkout", {
        method: "POST",
        body: JSON.stringify(address)
      });
      showConfirmation(receipt);
      await Promise.all([loadCart(), loadOrders(), loadProducts()]);
    } catch (error) {
      setFeedback(error.message, true);
    } finally {
      button.textContent = "Place order";
      renderCart();
    }
  });

  async function initialize() {
    try {
      await loadProducts();
      if (state.token) {
        try {
          const customer = await api("/api/account");
          setCustomer(customer, state.token);
          await Promise.all([loadCart(), loadOrders()]);
        } catch (error) {
          localStorage.removeItem(tokenKey);
          state.token = null;
          setCustomer(null, null);
          setFeedback(`Please sign in again: ${error.message}`, true);
        }
      }
      connection.classList.add("connected");
      connection.classList.remove("disconnected");
      document.querySelector("#connectionLabel").textContent = "Store connected";
    } catch (error) {
      connection.classList.add("disconnected");
      document.querySelector("#connectionLabel").textContent = "Store unavailable";
      document.querySelector("#productGrid").innerHTML =
        `<div class="empty-state">${escapeHtml(error.message)}. Start QueryFlow and check the database connection.</div>`;
    }
  }

  initialize();
})();
