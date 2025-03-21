// statsTable.js
import { fetchAgentStats } from './apiService.js';
import { updateCharts } from './charts.js';
import { displayContainers } from './containers.js';
import { displayGitlabRunner } from './gitlabRunner.js';
import { formatLastSeen } from './utils.js';

export let selectedAgent = null;
export let selectedHostname = null;

export function renderStatsTable(data) {
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
}

export function selectAgent(ip, hostname, rowElement) {
  selectedAgent = ip;
  selectedHostname = hostname;

  document.querySelectorAll("tr").forEach(tr => tr.classList.remove("selected"));
  rowElement.classList.add("selected");

  document.getElementById("chartsContainer").style.display = "block";
  document.getElementById("selectedServerInfo").innerHTML = `<b>${hostname}</b> (${ip})`;

  updateAgentData(ip);
}

async function updateAgentData(ip) {
  const data = await fetchAgentStats(ip);
  if (data.length === 0) return;

  let labels = data.map(entry => new Date(parseInt(entry.last_update)).toLocaleTimeString());
  let cpuData = data.map(entry => entry.cpu_usage);
  let memoryData = data.map(entry => entry.memory_used);

  if (labels.length > 100) {
    labels = labels.slice(-100);
    cpuData = cpuData.slice(-100);
    memoryData = memoryData.slice(-100);
  }
  
  updateCharts(labels, cpuData, memoryData);
  displayContainers(latestContainers(data), hostname, ip);
  displayGitlabRunner(data[0].gitlab_runner, hostname, ip);
}

function latestContainers(data) {
  for (let entry of data) {
    if (entry.containers && entry.containers.length > 0) return entry.containers;
  }
  return [];
}
