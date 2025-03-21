// main.js
import { fetchStats } from './apiService.js';
import { renderStatsTable, selectedAgent, selectedHostname } from './statsTable.js';

// Скрытие панелей при клике вне них
document.body.addEventListener("click", () => {
  document.getElementById("chartsContainer").style.display = "none";
  document.getElementById("containersContainer").style.display = "none";
  document.getElementById("gitlabRunnerContainer").style.display = "none";
  // Сброс выделения
  document.querySelectorAll("tr").forEach(tr => tr.classList.remove("selected"));
});

// Предотвращение закрытия панелей при клике по ним
document.getElementById("chartsContainer").addEventListener("click", (event) => {
  event.stopPropagation();
});

// Главный цикл обновления статистики
async function updateStats() {
  try {
    const data = await fetchStats();
    renderStatsTable(data);
  } catch (error) {
    console.error('Ошибка получения статистики:', error);
  }
}

// Начальный вызов и периодическое обновление
updateStats();
setInterval(updateStats, 2000);
