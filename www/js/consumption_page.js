window.onload = function() {
	authCheckAccessKey(initPage, redirectToLogin);
	addEventsNavbarBurgers();
}

function initPage() {
	userInfoFetch(function() {navbarPopulateItems("main-menu");});
	
	document.getElementById("button-submit").onclick = fetchConsumptionData;
	
	fetchApplianceList(function() { document.getElementById("button-submit").classList.remove("is-loading"); });
	
	window.smceeApplianceChart = new Chart(document.getElementById('appliance-chart-canvas'), {
		type: 'pie',
		data: {
			datasets: []
		},
		options: {
			responsive: true,
			animation: {
				duration: 0
			},
			aspectRatio: 1,
			layout: {
				padding: 20,
			},
			plugins: {
				legend: {
					display: false,
				}
			}
		}
	});
}

function tableClear() {
	var tableBody = document.getElementById("appliances-tbody");
	
	while(tableBody.lastChild)
		tableBody.removeChild(tableBody.lastChild);
}

function tableInsertItem(applianceName, energy, cost) {
	var tableNewRow = document.createElement("tr");
	var tableCellApplianceName = document.createElement("td");
	var tableCellApplianceEnergy = document.createElement("td");
	var tableCellApplianceCost = document.createElement("td");
	
	document.getElementById("appliances-tbody").appendChild(tableNewRow);
	
	tableNewRow.appendChild(tableCellApplianceName);
	tableNewRow.appendChild(tableCellApplianceEnergy);
	tableNewRow.appendChild(tableCellApplianceCost);
	
	tableCellApplianceName.className = "has-text-centered is-vcentered" + ((applianceName.length > 15) ? " is-size-7" : "");
	tableCellApplianceEnergy.className = "has-text-centered is-vcentered";
	tableCellApplianceCost.className = "has-text-centered is-vcentered";
	
	tableCellApplianceName.innerText = applianceName == null ? "(" + item.appliance_id + ")" : applianceName;
	tableCellApplianceEnergy.innerText = energy.toFixed(2) + " kWh";
	tableCellApplianceCost.innerText = "R$ " + cost.toFixed(2);
}

function fetchConsumptionData() {
	var dateStartInput = document.getElementById("input-date-start");
	var dateEndInput = document.getElementById("input-date-end");
	var xhrConsumptionData;
	var timestampStart, timestampEnd;
	
	timestampStart = new Date(dateStartInput.value).getTime() / 1000;
	timestampEnd = new Date(dateEndInput.value).getTime() / 1000;
	
	dateStartInput.classList.remove("is-danger");
	dateEndInput.classList.remove("is-danger");
	
	if(dateStartInput.value === "")
		dateStartInput.classList.add("is-danger");
	
	if(dateEndInput.value === "")
		dateEndInput.classList.add("is-danger");
	
	if(dateStartInput.value == "" || dateEndInput.value == "")
		return;
	
	if(timestampStart > timestampEnd) {
		dateStartInput.classList.add("is-danger");
		dateEndInput.classList.add("is-danger");
		return;
	}
	
	document.getElementById("button-submit").classList.add("is-loading");
	
	tableClear();
	
	xhrConsumptionData = new XMLHttpRequest()
	
	xhrConsumptionData.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			var totalApplianceEnergy = 0, totalApplianceCost = 0;
			
			document.getElementById("consumption-total-energy").innerText = responseObject.total_energy.toFixed(2);
			document.getElementById("consumption-total-cost").innerText = responseObject.total_cost.toFixed(2);
			
			document.getElementById("consumption-standby-energy").innerText = responseObject.standby_energy.toFixed(2);
			document.getElementById("consumption-standby-cost").innerText = responseObject.standby_energy.toFixed(2);
			
			if(responseObject.second_count / (timestampEnd - timestampStart) < 0.99) {
				document.getElementById("coverage-percentage").innerText = (responseObject.second_count * 100 / (timestampEnd - timestampStart)).toFixed(1);
				document.getElementById("low-coverage-warning").classList.remove("is-hidden");
			} else {
				document.getElementById("low-coverage-warning").classList.add("is-hidden");
			}
			
			window.smceeApplianceChart.data.datasets = [];
			
			window.smceeApplianceChart.data.labels = [];
			window.smceeApplianceChart.data.datasets.push({
				label: 'Energia',
				backgroundColor: [],
				data: [],
			});
			
			window.smceeApplianceChart.data.datasets[0].data.push(responseObject.standby_energy);
			window.smceeApplianceChart.data.datasets[0].backgroundColor.push('#3c3c3c');
			window.smceeApplianceChart.data.labels.push('Stand-by');
			
			for(let applianceId = 0; applianceId < responseObject.appliance_energy.length; applianceId++)
				if(typeof responseObject.appliance_energy[applianceId] == "number" && typeof responseObject.appliance_cost[applianceId] == "number") {
					tableInsertItem(window.smceeApplianceList.get(applianceId + 1).name, responseObject.appliance_energy[applianceId], responseObject.appliance_cost[applianceId]);
					
					totalApplianceEnergy += responseObject.appliance_energy[applianceId];
					totalApplianceCost += responseObject.appliance_cost[applianceId];
					
					window.smceeApplianceChart.data.datasets[0].data.push(responseObject.appliance_energy[applianceId]);
					window.smceeApplianceChart.data.datasets[0].backgroundColor.push(window.smceeApplianceList.get(applianceId + 1).color);
					window.smceeApplianceChart.data.labels.push(window.smceeApplianceList.get(applianceId + 1).name);
				}
			
			document.getElementById("consumption-unknown-energy").innerText = (responseObject.total_energy - totalApplianceEnergy).toFixed(2);
			document.getElementById("consumption-unknown-cost").innerText = (responseObject.total_cost - totalApplianceCost).toFixed(2);
			
			window.smceeApplianceChart.data.datasets[0].data.push(responseObject.total_energy - totalApplianceEnergy);
			window.smceeApplianceChart.data.datasets[0].backgroundColor.push('#b4b4b4');
			window.smceeApplianceChart.data.labels.push('Desconhecido');
			
			if(responseObject.appliance_energy.length > 0)
				document.getElementById("appliance-chart-table").classList.remove("is-hidden");
			else
				document.getElementById("appliance-chart-table").classList.add("is-hidden");
			
			window.smceeApplianceChart.update();
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch consumption data. Status: " + this.status);
		}
		
		document.getElementById("button-submit").classList.remove("is-loading");
	};
	
	xhrConsumptionData.open("GET", window.smceeApiUrlBase + "energy/consumption?start=" + timestampStart + "&end=" + timestampEnd, true);
	
	xhrConsumptionData.timeout = 2000;
	
	xhrConsumptionData.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrConsumptionData.send();
}
