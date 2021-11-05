window.onload = function() {
	authCheckAccessKey(initPage, redirectToLogin);
	addEventsNavbarBurgers();
	
	window.smceePowerDataHours = 4;
}

function initPage() {
	userInfoFetch(function() {navbarPopulateItems("main-menu");});
	
	document.getElementById("chart-type-select").onchange = updateSignatureChart;
	document.getElementById("event-type-select").onchange = updateSignatureChart;
	document.getElementById("appliance-select").onchange = updateSignatureChart;
	
	window.smceeSignatureScatterChart = new Chart(document.getElementById("signature-chart-canvas"), {
		type: "scatter",
		data: {
			datasets: []
		},
		options: {
			title: {
				display: true,
				text: "Assinaturas de Aparelhos",
			},
			responsive: true,
			maintainAspectRatio: false,
			animation: {
				duration: 0
			}
		}
	});
	
	fetchApplianceList(function() { fetchSignatures(); updateApplianceSelect(); });
}

function updateSignatureChart() {
	var chartType = document.getElementById("chart-type-select").value;
	var eventType = document.getElementById("event-type-select").value;
	var applianceId = Number(document.getElementById("appliance-select").value);
	var activeData = [];
	var inactiveData = [];
	var selectedData = [];
	
	if(chartType === "pxpk") {
		document.getElementById("event-type-select").disabled = true;
		eventType = "on";
	} else {
		document.getElementById("event-type-select").disabled = false;
	}
	
	if(typeof window.smceeApplianceList != "object" || typeof window.smceeSignatures != "object")
		return;
	
	window.smceeSignatureScatterChart.datasets = [];
	
	for(signatureItem of window.smceeSignatures) {
		let value;
		
		if((eventType === "on" && signatureItem.delta_pt < 0) || (eventType === "off" && signatureItem.delta_pt >= 0))
			continue;
		
		if(chartType === "pxq")
			value = {x: signatureItem.delta_pt, y: (signatureItem.delta_q[0] + signatureItem.delta_q[1])};
		else if(chartType === "pxpk")
			value = {x: signatureItem.delta_pt, y: signatureItem.peak_pt};
		else if(chartType === "pxd")
			value = {x: signatureItem.delta_pt, y: signatureItem.duration};
		else if(chartType === "pxs")
			value = {x: signatureItem.delta_pt, y: (signatureItem.delta_s[0] + signatureItem.delta_s[1])};
		else if(chartType === "p1xp2")
			value = {x: signatureItem.delta_p[0], y: signatureItem.delta_p[1]};
		
		if(signatureItem.appliance_id === applianceId)
			selectedData.push(value);
		else if(window.smceeApplianceList.get(signatureItem.appliance_id).is_active)
			activeData.push(value);
		else
			inactiveData.push(value);
	}
	
	window.smceeSignatureScatterChart.data.datasets = [{
			label: "Aparelhos ativos",
			backgroundColor: 'rgba(100, 100, 255)',
			data: activeData
		}];
	
	if(inactiveData.length > 0)
		window.smceeSignatureScatterChart.data.datasets.push({
			label: "Aparelhos inativos",
			backgroundColor: 'rgba(200, 200, 200)',
			data: inactiveData
		});
	
	if(applianceId > 0)
		window.smceeSignatureScatterChart.data.datasets.push({
			label: "Aparelho selecionado",
			backgroundColor: 'rgba(255, 100, 100)',
			data: selectedData
		});
	
	window.smceeSignatureScatterChart.update();
}

function updateApplianceSelect() {
	var applianceSelectElement = document.getElementById("appliance-select");
	var selectOptionElement;
	
	if(typeof window.smceeApplianceList != "object" || applianceSelectElement === null)
		return;
	
	while(applianceSelectElement.childElementCount > 1)
		applianceSelectElement.removeChild(applianceSelectElement.lastChild);
	
	for(applianceItem of window.smceeApplianceList.values()) {
		selectOptionElement = document.createElement("option");
		selectOptionElement.innerText = applianceItem.name + (applianceItem.is_active ? "" : " (inativo)");
		selectOptionElement.value = applianceItem.id;
		
		applianceSelectElement.appendChild(selectOptionElement);
	}
}

function fetchSignatures() {
	var xhrSignatures = new XMLHttpRequest();
	
	xhrSignatures.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			window.smceeSignatures = [];
			
			for(signature of responseObject)
				window.smceeSignatures.push(signature);
			
			updateSignatureChart();
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch signatures. Status: " + this.status);
		}
	}
	
	xhrSignatures.open("GET", window.smceeApiUrlBase + "appliances/signatures");
	
	xhrSignatures.timeout = 2000;
	
	xhrSignatures.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrSignatures.send();
}
