// apiService.js
export async function fetchStats() {
    const response = await fetch('/stats');
    return response.json();
  }
  
  export async function fetchAgentStats(ip) {
    const response = await fetch(`/stats/${ip}`);
    return response.json();
  }
  