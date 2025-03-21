// containers.js
export function displayContainers(containers, hostname, ip) {
    const containersContainer = document.getElementById("containersContainer");
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
  