// gitlabRunner.js
export function displayGitlabRunner(gitlabRunner, hostname, ip) {
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
  