window.onload = function() {
	authCheckAccessKey(initPage, redirectToLogin);
	addEventsNavbarBurgers();
	
	window.smceePowerDataHours = 4;
}

function initPage() {
	userInfoFetch(function() {navbarPopulateItems("main-menu");});
	
	document.getElementById("event-type-select").value = "all";
	
	document.getElementById("chart-type-select").onchange = updateSignatureChart;
	document.getElementById("event-type-select").onchange = updateSignatureChart;
	
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
			},
			plugins: {
				legend: {
					display: true,
					onClick: handleLegendClick,
				}
			}
		}
	});
	
	fetchApplianceList(fetchSignatures);
}

function handleLegendClick(evt, item, legend) {
	if(typeof window.smceeHighlightedDataset == "string" && item.text === window.smceeHighlightedDataset) {
		legend.chart.data.datasets.forEach((dataset) => {
			dataset.backgroundColor = (dataset.backgroundColor.length === 9) ? dataset.backgroundColor.slice(0, -2) : dataset.backgroundColor;
			dataset.borderWidth = 1;
		});
		
		window.smceeHighlightedDataset = null;
	} else {
		legend.chart.data.datasets.forEach((dataset) => {
			if(dataset.label === item.text) {
				dataset.backgroundColor = (dataset.backgroundColor.length === 9) ? dataset.backgroundColor.slice(0, -2) : dataset.backgroundColor;
			} else {
				dataset.backgroundColor = dataset.backgroundColor + ((dataset.backgroundColor.length === 9) ? '' : '10');
			}
			dataset.borderWidth = 0;
		});
		
		window.smceeHighlightedDataset = item.text;
	}
	
	legend.chart.update();
}

function updateSignatureChart() {
	var chartType = document.getElementById("chart-type-select").value;
	var eventType = document.getElementById("event-type-select").value;
	var applianceSignatures = new Map();
	var inactiveAppliancesData = [];
	
	if(chartType === "pxpk") {
		document.getElementById("event-type-select").disabled = true;
		eventType = "on";
	} else {
		document.getElementById("event-type-select").disabled = false;
	}
	
	if(typeof window.smceeApplianceList != "object" || typeof window.smceeSignatures != "object")
		return;
	
	window.smceeSignatureScatterChart.data.datasets = [];
	
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
		
		if(!window.smceeApplianceList.get(signatureItem.appliance_id).is_active) {
			inactiveAppliancesData.push(value);
		} else {
			if(!applianceSignatures.has(signatureItem.appliance_id))
				applianceSignatures.set(signatureItem.appliance_id, new Array())
			
			applianceSignatures.get(signatureItem.appliance_id).push(value);
		}
	}
	
	for(const applianceId of applianceSignatures.keys())
		window.smceeSignatureScatterChart.data.datasets.push({
			label: window.smceeApplianceList.get(applianceId).name,
			backgroundColor: window.smceeApplianceList.get(applianceId).color,
			data: applianceSignatures.get(applianceId)
		});
	
	if(inactiveAppliancesData.length > 0)
		window.smceeSignatureScatterChart.data.datasets.push({
			label: "Aparelhos inativos",
			backgroundColor: "#a3a3a3",
			data: inactiveAppliancesData
		});
	
	window.smceeSignatureScatterChart.update();
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
