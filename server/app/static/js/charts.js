// charts.js
export const cpuChartCtx = document.getElementById('cpuChart').getContext('2d');
export const memoryChartCtx = document.getElementById('memoryChart').getContext('2d');

export let cpuChart = new Chart(cpuChartCtx, {
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
    scales: { y: { beginAtZero: true, max: 100 } }
  }
});

export let memoryChart = new Chart(memoryChartCtx, {
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
    scales: { y: { beginAtZero: true } }
  }
});

// Функции для обновления графиков можно также экспортировать
export function updateCharts(labels, cpuData, memoryData) {
  cpuChart.data.labels = labels;
  cpuChart.data.datasets[0].data = cpuData;
  cpuChart.update();

  memoryChart.data.labels = labels;
  memoryChart.data.datasets[0].data = memoryData;
  memoryChart.update();
}
