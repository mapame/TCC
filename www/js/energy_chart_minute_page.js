window.onload = function() {
	authCheckAccessKey(initPage, redirectToLogin);
	addEventsNavbarBurgers();
}

function initPage() {
	var dateMax = new Date();
	var dateMin = new Date(dateMax - 3 * 30 * 24 * 3600 * 1000);
	
	userInfoFetch(function() {navbarPopulateItems("main-menu");});
	
	window.smceeEnergyData = {
		date: "",
		data: [],
		disaggregated: false
	};
	
	
	document.getElementById("date-input").min = dateMin.getFullYear() + "-" + (dateMin.getMonth() + 1).toString().padStart(2, "0") + "-" +  dateMin.getDate().toString().padStart(2, "0");
	document.getElementById("date-input").max = dateMax.getFullYear() + "-" + (dateMax.getMonth() + 1).toString().padStart(2, "0") + "-" +  dateMax.getDate().toString().padStart(2, "0");
	document.getElementById("date-input").value = document.getElementById("date-input").max;
	
	document.getElementById("disaggregation-checkbox").checked = false;
	document.getElementById("reactive-checkbox").checked = false;
	document.getElementById("stacked-checkbox").checked = false;
	
	document.getElementById("date-input").onchange = updateChart;
	document.getElementById("disaggregation-checkbox").onchange = updateChart;
	document.getElementById("reactive-checkbox").onchange = updateChart;
	document.getElementById("stacked-checkbox").onchange = updateChart;
	
	
	window.smceeEnergyChart = new Dygraph(document.getElementById("minute-energy-chart"), [[0]], {
		labels: [""],
		xValueParser: function(x) {return x;},
		drawPoints: false,
		legend: "always",
		includeZero: true,
		labelsShowZeroValues: false,
		fillGraph: true,
		highlightSeriesBackgroundAlpha: 0.6,
		highlightSeriesOpts: {
			strokeWidth: 2,
			strokeBorderWidth: null,
		},
		axes: {
			y: {
				axisLabelWidth: 20,
				valueFormatter: function(y, opts, series_name) { return y.toFixed(1) + (series_name === "Energia Reativa" ? " VArh" : " Wh"); }
			},
		},
	});
	
	updateChart();
	fetchApplianceList(function() { document.getElementById("disaggregation-checkbox").disabled = false; });
}

function updateChart() {
	var disaggregatedEnergyMode = document.getElementById("disaggregation-checkbox").checked;
	var dateInputField = document.getElementById('date-input');
	
	if(dateInputField.value === "" && window.smceeEnergyData.date !== "")
		dateInputField.value = window.smceeEnergyData.date;
	
	if(window.smceeEnergyData.date != dateInputField.value || window.smceeEnergyData.disaggregated != disaggregatedEnergyMode) {
		fetchEnergyData(disaggregatedEnergyMode);
		
		window.smceeEnergyChart.resetZoom();
	} else {
		if(disaggregatedEnergyMode) {
			window.smceeEnergyChart.setVisibility(new Array(window.smceeEnergyChart.numColumns() - 1).fill(true));
			
			window.smceeEnergyChart.updateOptions({
				'stackedGraph' : document.getElementById("stacked-checkbox").checked
			});
		} else {
			window.smceeEnergyChart.setVisibility([true, document.getElementById("reactive-checkbox").checked]);
		}
	}
}

function generateEnergyFile(energyData, startTimestamp, endTimestamp) {
	var lastTimestamp = null;
	var fileData = [];
	
	fileData.push([new Date((startTimestamp - 1) * 1000), null, null]);
	
	for(const element of energyData) {
		if(element.second_count < 15)
			continue;
		
		if(lastTimestamp !== null && element.timestamp - lastTimestamp > 60)
			fileData.push([new Date((element.timestamp - 1) * 1000), null, null]);
		
		fileData.push([new Date(element.timestamp * 1000), element.active * 1000, element.reactive * 1000])
		
		lastTimestamp = element.timestamp;
	}
	
	fileData.push([new Date((endTimestamp + 1) * 1000), null, null]);
	
	return fileData;
}

function generateDisaggregatedEnergyFile(energyData, applianceIdSet, startTimestamp, endTimestamp) {
	var lastTimestamp = null;
	var fileData = [];
	var energyEntry;
	var disaggregatedEnergySum;
	var excessPowerCounter = 0;
	
	fileData.push([new Date((startTimestamp - 1) * 1000)].concat(new Array(applianceIdSet.size + 1).fill(null)));
	
	for(const element of energyData) {
		if(lastTimestamp !== null && element.timestamp - lastTimestamp > 60)
			fileData.push([new Date((element.timestamp - 1) * 1000)].concat(new Array(applianceIdSet.size + 1).fill(null)));
		
		disaggregatedEnergySum = element.standby_energy;
		
		energyEntry = new Array();
		
		energyEntry.push(new Date(element.timestamp * 1000));
		
		for(const applianceId of applianceIdSet.values()) {
			if(typeof element.appliance_energy[applianceId - 1] == "undefined") {
				energyEntry.push(0);
			} else {
				energyEntry.push(element.appliance_energy[applianceId - 1] * 1000);
				disaggregatedEnergySum += element.appliance_energy[applianceId - 1];
			}
		}
		
		if(element.total_energy < disaggregatedEnergySum) {
			energyEntry.push(0);
			
			excessPowerCounter++;
		} else {
			energyEntry.push((element.total_energy - disaggregatedEnergySum) * 1000);
		}
		
		fileData.push(energyEntry);
		
		lastTimestamp = element.timestamp;
	}
	
	if(excessPowerCounter > 0)
		console.warn(excessPowerCounter + " minutos com potência excedente.");
	
	fileData.push([new Date((endTimestamp + 1) * 1000)].concat(new Array(applianceIdSet.size + 1).fill(null)));
	
	return fileData;
}

function findAppliancesInEnergy(energyData) {
	var applianceIdSet = new Set();
	
	for(const element of energyData)
		for(let applianceId = 1; applianceId <= element.appliance_energy.length; applianceId++)
			if(typeof element.appliance_energy[applianceId - 1] == "number" && element.appliance_energy[applianceId - 1] > 0)
				applianceIdSet.add(applianceId);
	
	return applianceIdSet;
}

function fetchEnergyData(disaggregatedEnergy=false) {
	var selectedDate = document.getElementById('date-input').value.split("-");
	var startTimestamp = (new Date(Number(selectedDate[0]), Number(selectedDate[1]) - 1, Number(selectedDate[2]))) / 1000;
	var endTimestamp = startTimestamp + 24 * 3600 - 1;
	var xhrEnergyData = new XMLHttpRequest();
	
	document.getElementById('date-input').disabled = true;
	document.getElementById('disaggregation-checkbox').disabled = true;
	document.getElementById("reactive-checkbox").disabled = true;
	document.getElementById("stacked-checkbox").disabled = true;
	
	xhrEnergyData.onload = function() {
		if(this.status == 200) {
			let responseObj = JSON.parse(this.responseText);
			var labels = ["Hora"];
			var colors = [];
			var lastTimestamp = null;
			var currentEntry;
			
			window.smceeEnergyData.date = selectedDate.join("-");
			window.smceeEnergyData.disaggregated = disaggregatedEnergy;
			
			if(disaggregatedEnergy) {
				var applianceIdSet;
				
				applianceIdSet = findAppliancesInEnergy(responseObj);
				
				for(const applianceId of applianceIdSet.values()) {
					const appliance = window.smceeApplianceList.get(applianceId);
					
					labels.push(appliance.name);
					colors.push(appliance.color);
				}
				
				labels.push("Desconhecido");
				colors.push("#636363");
				
				window.smceeEnergyData.data = generateDisaggregatedEnergyFile(responseObj, applianceIdSet, startTimestamp, endTimestamp);
			} else {
				labels.push("Energia", "Energia Reativa");
				
				window.smceeEnergyData.data = generateEnergyFile(responseObj, startTimestamp, endTimestamp);
			}
			
			window.smceeEnergyChart.updateOptions({
				'stackedGraph' : disaggregatedEnergy && document.getElementById("stacked-checkbox").checked,
				'labels' : labels,
				'colors': (disaggregatedEnergy ? colors : null),
				'file' : window.smceeEnergyData.data
			});
			
			if(disaggregatedEnergy)
				window.smceeEnergyChart.setVisibility(new Array(window.smceeEnergyChart.numColumns() - 1).fill(true));
			else
				window.smceeEnergyChart.setVisibility([true, document.getElementById("reactive-checkbox").checked]);
			
			document.getElementById('date-input').disabled = false;
			document.getElementById('disaggregation-checkbox').disabled = (typeof window.smceeApplianceList == "undefined");
			document.getElementById("reactive-checkbox").disabled = disaggregatedEnergy;
			document.getElementById("stacked-checkbox").disabled = !disaggregatedEnergy;
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch energy data. Status: " + this.status);
		}
	}
	
	xhrEnergyData.open("GET", window.smceeApiUrlBase + (disaggregatedEnergy ? "disaggregated_energy" : "energy") + "/minutes?start=" + startTimestamp + "&end=" + endTimestamp);
	
	xhrEnergyData.timeout = 2000;
	
	xhrEnergyData.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrEnergyData.send();
}
