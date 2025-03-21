// static/js/main.js

const cpuChartCtx = document.getElementById('cpuChart').getContext('2d');
const memoryChartCtx = document.getElementById('memoryChart').getContext('2d');
const chartsContainer = document.getElementById("chartsContainer");
const containersContainer = document.getElementById("containersContainer");

let cpuChart = new Chart(cpuChartCtx, {
  type: 'line',
  data: {
    labels: [],
    datasets: [{
      label: 'CPU Usage (%)',
      data: [],
      borderColor: 'red',
      backgroundColor: 'rgba(255, 0, 0, 0.2)',
      fill: true
    }]
  },
  options: {
    responsive: true,
    scales: {
      y: { beginAtZero: true, max: 100 }
    }
  }
});

let memoryChart = new Chart(memoryChartCtx, {
  type: 'line',
  data: {
    labels: [],
    datasets: [{
      label: 'Memory Usage (MB)',
      data: [],
      borderColor: 'blue',
      backgroundColor: 'rgba(0, 0, 255, 0.2)',
      fill: true
    }]
  },
  options: {
    responsive: true,
    scales: {
      y: { beginAtZero: true }
    }
  }
});

let selectedAgent = null;
let selectedHostname = null;

function formatLastSeen(seconds) {
  if (seconds < 60) return seconds + " sec ago";
  else if (seconds < 3600) return Math.floor(seconds / 60) + " min ago";
  else if (seconds < 86400) return Math.floor(seconds / 3600) + " hr ago";
  else if (seconds < 604800) return Math.floor(seconds / 86400) + " days ago";
  else if (seconds < 2592000) return Math.floor(seconds / 604800) + " weeks ago";
  else if (seconds < 31536000) return Math.floor(seconds / 2592000) + " months ago";
  else return Math.floor(seconds / 31536000) + " years ago";
}

function fetchStats() {
  fetch('/stats')
    .then(response => response.json())
    .then(data => {
      const tableBody = document.getElementById("stats-body");
      tableBody.innerHTML = "";

      data.forEach(row => {
        const lastSeen = Math.round(row.last_seen);
        const isOffline = lastSeen > 180;

        const tr = document.createElement("tr");
        tr.innerHTML = `
          <td>${row.hostname}</td>
          <td>${row.ip_address}</td>
          <td>${row.os_info}</td>
          <td>${row.cpu_usage}</td>
          <td>${row.memory_total - row.memory_used} / ${row.memory_used} / ${row.memory_total}</td>
          <td>${row.disk_free} / ${row.disk_total - row.disk_free} / ${row.disk_total}</td>
          <td>${formatLastSeen(lastSeen)}</td>
        `;
        tr.classList.toggle("offline", isOffline);
        tr.style.color = isOffline ? "red" : "white";

        tr.addEventListener("click", (event) => {
          event.stopPropagation();
          selectAgent(row.ip_address, row.hostname, tr);
        });

        if (row.ip_address === selectedAgent) {
          tr.classList.add("selected");
        }

        tableBody.appendChild(tr);
      });

      if (selectedAgent && selectedHostname) {
        fetchAgentStats(selectedAgent, selectedHostname);
      }
    })
    .catch(error => console.error('Error fetching stats:', error));
}

function selectAgent(ip, hostname, rowElement) {
  selectedAgent = ip;
  selectedHostname = hostname;

  document.querySelectorAll("tr").forEach(tr => tr.classList.remove("selected"));
  rowElement.classList.add("selected");

  chartsContainer.style.display = "block";
  document.getElementById("selectedServerInfo").innerHTML = `<b>${hostname}</b> (${ip})`;

  fetchAgentStats(ip, hostname);
}

function fetchAgentStats(ip, hostname) {
  fetch(`/stats/${ip}`)
    .then(response => response.json())
    .then(data => {
      if (data.length === 0) return;

      let labels = data.map(entry => new Date(parseInt(entry.last_update)).toLocaleTimeString());
      let cpuData = data.map(entry => entry.cpu_usage);
      let memoryData = data.map(entry => entry.memory_used);

      if (labels.length > 100) {
        labels = labels.slice(-100);
        cpuData = cpuData.slice(-100);
        memoryData = memoryData.slice(-100);
      }

      cpuChart.data.labels = labels;
      cpuChart.data.datasets[0].data = cpuData;
      cpuChart.update();

      memoryChart.data.labels = labels;
      memoryChart.data.datasets[0].data = memoryData;
      memoryChart.update();

      const latestRecord = data[0];

      displayContainers(latestContainers(data), hostname, ip);
      displayGitlabRunner(latestRecord.gitlab_runner, hostname, ip);
    })
    .catch(error => console.error('Ошибка загрузки данных:', error));
}

function latestContainers(data) {
  for (let entry of data) {
    if (entry.containers && entry.containers.length > 0) return entry.containers;
  }
  return [];
}

function displayContainers(containers, hostname, ip) {
  const containersServerName = document.getElementById("containersServerName");
  const containersBody = document.getElementById("containers-body");

  containersContainer.style.display = "block";
  containersServerName.innerText = `${hostname} (${ip})`;
  containersBody.innerHTML = '';

  if (containers.length > 0) {
    containers.forEach(container => {
      const tr = document.createElement("tr");
      tr.innerHTML = `
        <td>${container.id}</td>
        <td>${container.image}</td>
        <td>${container.name}</td>
        <td>${container.status}</td>
        <td>${container.cpu_perc}</td>
        <td>${container.mem_usage}</td>
      `;
      containersBody.appendChild(tr);
    });
  } else {
    containersBody.innerHTML = '<tr><td colspan="6">Контейнеры не найдены.</td></tr>';
  }
}

function displayGitlabRunner(gitlabRunner, hostname, ip) {
  const gitlabRunnerContainer = document.getElementById("gitlabRunnerContainer");
  const gitlabRunnerServerName = document.getElementById("gitlabRunnerServerName");
  const gitlabRunnerError = document.getElementById("gitlabRunnerError");
  const gitlabRunnerData = document.getElementById("gitlabRunnerData");

  const runnerServiceStatus = document.getElementById("runnerServiceStatus");
  const runnerInstalled = document.getElementById("runnerInstalled");
  const runnerVersion = document.getElementById("runnerVersion");
  const runnerStartTime = document.getElementById("runnerStartTime");

  const gitlabRunnerTableBody = document.getElementById("gitlabRunnerTableBody");
  const gitlabRunnerLogTableBody = document.getElementById("gitlabRunnerLogTableBody");

  gitlabRunnerContainer.style.display = "none";
  gitlabRunnerError.textContent = "";
  gitlabRunnerData.style.display = "none";

  if (!gitlabRunner || gitlabRunner.installed === false) {
    gitlabRunnerError.textContent = "GitLab Runner отсутствует на сервере...";
    gitlabRunnerContainer.style.display = "block";
    return;
  }

  gitlabRunnerContainer.style.display = "block";
  gitlabRunnerData.style.display = "block";
  gitlabRunnerServerName.innerText = `${hostname} (${ip})`;

  runnerServiceStatus.textContent = gitlabRunner.active || "unknown";
  runnerInstalled.textContent = gitlabRunner.installed ? "Да" : "Нет";
  runnerVersion.textContent = gitlabRunner.version || "-";
  runnerStartTime.textContent = gitlabRunner.start_time || "-";

  gitlabRunnerTableBody.innerHTML = "";
  const runners = gitlabRunner.runners || [];
  if (runners.length > 0) {
    runners.forEach(r => {
      const tr = document.createElement("tr");
      tr.innerHTML = `
        <td>${r.name}</td>
        <td>${r.executor}</td>
        <td>${r.url}</td>
      `;
      gitlabRunnerTableBody.appendChild(tr);
    });
  } else {
    gitlabRunnerTableBody.innerHTML = '<tr><td colspan="3">Нет зарегистрированных раннеров</td></tr>';
  }

  gitlabRunnerLogTableBody.innerHTML = "";
  if (Array.isArray(gitlabRunner.job)) {
    gitlabRunner.job.forEach(line => {
      const tr = document.createElement("tr");
      const td = document.createElement("td");
      td.textContent = line;
      tr.appendChild(td);
      gitlabRunnerLogTableBody.appendChild(tr);
    });
  } else {
    const tr = document.createElement("tr");
    const td = document.createElement("td");
    td.textContent = "Лог отсутствует или не в массиве.";
    tr.appendChild(td);
    gitlabRunnerLogTableBody.appendChild(tr);
  }
}

document.body.addEventListener("click", () => {
  chartsContainer.style.display = "none";
  containersContainer.style.display = "none";
  document.getElementById("gitlabRunnerContainer").style.display = "none";
  selectedAgent = null;
  document.querySelectorAll("tr").forEach(tr => tr.classList.remove("selected"));
});

chartsContainer.addEventListener("click", (event) => {
  event.stopPropagation();
});

fetchStats();
setInterval(fetchStats, 2000);
