(() => {
  const state = { lastEventId: 0, refreshActive: false, events: [] };

  function escapeHtml(value) {
    return String(value ?? "").replace(/[&<>"']/g, (character) => ({
      "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
    })[character]);
  }

  function setText(selector, value) {
    document.querySelector(selector).textContent = value;
  }

  function formatMs(value) {
    return value === null || value === undefined
      ? "-" : `${Number(value).toFixed(1)} ms`;
  }

  function formatTime(value) {
    if (!value) return "-";
    return new Date(value).toLocaleTimeString([], {
      hour: "2-digit", minute: "2-digit", second: "2-digit"
    });
  }

  async function api(path) {
    const response = await fetch(path);
    const body = await response.json();
    if (!response.ok) throw new Error(body.error || `Request failed (${response.status})`);
    return body;
  }

  function renderQueue(queue, policy) {
    setText("#queueCount", queue.length);
    setText("#queuePolicyHint", policy === "priority"
      ? "Priority high → low · ties FCFS" : "First-come, first-served");
    const body = document.querySelector("#queueBody");
    if (!queue.length) {
      body.innerHTML = '<tr><td colspan="6" class="table-empty">Queue is empty</td></tr>';
      return;
    }
    body.innerHTML = queue.map((item) => `<tr>
      <td>${item.position}</td><td class="table-query-id">Q${item.id}</td>
      <td>${escapeHtml(item.userId || "system")}</td>
      <td>${escapeHtml(item.operation)}</td><td class="table-priority">${item.priority}</td>
      <td>${formatMs(item.waitingMs)}</td>
    </tr>`).join("");
  }

  function renderWorkers(workers) {
    document.querySelector("#workerList").innerHTML = workers.map((worker) => `
      <article class="worker-card ${worker.state === "busy" ? "busy" : ""}">
        <div class="worker-top"><b>${escapeHtml(worker.workerId)}</b>
          <span class="worker-state">${escapeHtml(worker.state)}</span></div>
        <p class="worker-assignment">${worker.queryId
          ? `Processing Q${worker.queryId} · ${escapeHtml(worker.operation)}`
          : "Waiting for work"}</p>
        <span class="worker-timing">${worker.state === "busy"
          ? formatMs(worker.executionMs)
          : `${worker.completedQueries} completed`}</span>
      </article>`).join("");
  }

  function renderEvents(events) {
    const list = [...events].reverse().slice(0, 100);
    const target = document.querySelector("#eventList");
    if (!list.length) {
      target.innerHTML = '<div class="empty-state">Customer actions will appear here as they enter the engine.</div>';
      return;
    }
    target.innerHTML = list.map((event) => `<div class="event-row">
      <time class="event-time">${formatTime(event.timestamp)}</time>
      <span class="event-type">${escapeHtml(event.type)}</span>
      <span class="event-detail">Q${event.queryId || "-"} ${event.workerId
        ? `· ${escapeHtml(event.workerId)}` : ""} ${event.userId
        ? `· customer ${escapeHtml(event.userId)}` : ""} · ${escapeHtml(event.detail)}</span>
    </div>`).join("");
  }

  function renderQueries(queries) {
    const target = document.querySelector("#queryList");
    const recent = [...queries].reverse().slice(0, 60);
    if (!recent.length) {
      target.innerHTML = '<div class="empty-state">Browsing, cart, sign-in, and checkout operations appear here.</div>';
      return;
    }
    target.innerHTML = recent.map((query) => `<article class="query-item">
      <div class="query-title-row"><div class="query-title">
        <span class="query-id">Q${query.id}</span>
        <span class="status status-${escapeHtml(query.status)}">${escapeHtml(query.status)}</span>
        <span class="query-operation">${escapeHtml(query.operation)}</span>
      </div><span class="query-priority">P${query.priority}</span></div>
      <p class="query-sql">${escapeHtml(query.sql)}</p>
      <div class="query-meta"><span>Customer ${escapeHtml(query.userId || "system")}</span>
        <span>${escapeHtml(query.workerId || "awaiting worker")}</span>
        <span>Wait ${formatMs(query.waitingMs)}</span>
        <span>Execution ${formatMs(query.executionMs)}</span></div>
      ${query.error ? `<div class="query-result error">${escapeHtml(query.error)}</div>` : ""}
    </article>`).join("");
  }

  function renderMetrics(metrics, events) {
    setText("#metricTotal", metrics.totalSubmitted);
    setText("#metricQueued", metrics.queued);
    setText("#metricRunning", metrics.running);
    setText("#metricCompleted", metrics.completed);
    setText("#metricFailed", metrics.failed);
    setText("#metricWorkers", `${metrics.activeWorkers} / ${metrics.workerCount}`);
    setText("#metricThroughput", Number(metrics.cumulativeThroughputPerSecond).toFixed(2));
    setText("#metricWait", formatMs(metrics.averageWaitingMs));
    setText("#metricExecution", formatMs(metrics.averageExecutionMs));
    setText("#metricResponse", formatMs(metrics.averageResponseMs));
    setText("#stageSubmitted", metrics.totalSubmitted);
    setText("#stageAssigned", metrics.totalSubmitted);
    setText("#stageQueue", metrics.queueLength);
    setText("#stageScheduled", events.filter((event) =>
      event.type === "query.scheduled").length);
    setText("#stageWorkers", metrics.activeWorkers);
    setText("#stageTerminal", metrics.completed + metrics.failed);
    setText("#pipelineCaption", metrics.totalSubmitted
      ? `${metrics.totalSubmitted} operations observed in this process`
      : "Waiting for storefront activity");
  }

  async function refresh() {
    if (state.refreshActive) return;
    state.refreshActive = true;
    try {
      const [config, queries, queue, workers, events, metrics] = await Promise.all([
        api("/api/config"),
        api("/api/queries"),
        api("/api/queue"),
        api("/api/workers"),
        api(`/api/events?after=${state.lastEventId}`),
        api("/api/metrics")
      ]);
      if (events.length) state.lastEventId = events[events.length - 1].eventId;
      state.events = [...(state.events || []), ...events].slice(-2000);
      setText("#backendTag", `${config.backend} · persistent store`);
      setText("#adminPolicyTag", config.policy === "priority"
        ? "Priority + FCFS" : "Pure FCFS");
      setText("#workerCountValue", config.workerCount);
      renderQueue(queue, config.policy);
      renderWorkers(workers);
      renderEvents(state.events);
      renderQueries(queries);
      renderMetrics(metrics, state.events);
      const connection = document.querySelector("#connectionStatus");
      connection.classList.add("connected");
      connection.classList.remove("disconnected");
      setText("#connectionLabel", "Engine connected");
    } catch (error) {
      const connection = document.querySelector("#connectionStatus");
      connection.classList.remove("connected");
      connection.classList.add("disconnected");
      setText("#connectionLabel", `Connection error: ${error.message}`);
    } finally {
      state.refreshActive = false;
    }
  }

  refresh();
  window.setInterval(refresh, 500);
})();
