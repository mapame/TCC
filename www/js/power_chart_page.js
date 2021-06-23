window.onload = function () {
	authCheckAccessKey(initPage, redirectToLogin);
	addEventsNavbarBurgers();
	
	document.getElementById("button-show-voltage-container").onclick = showVoltageChart;
	document.getElementById("checkbox-display-events").onchange = changeDisplayEvents;
	
	window.smceePowerChart = new Dygraph(document.getElementById("power-chart"), [[0, null]], {
		labels: ["Hora", "Potência"],
		xValueParser: function(x) {return x;},
		drawPoints: false,
		labelsDiv: document.getElementById("power-chart-labels"),
		legend: "always",
		includeZero: true,
		axes: {
			y: {
				axisLabelFormatter: function(x) { return x + " W"; }
			}
		}
	});
	
	window.smceeVoltageChart = new Dygraph(document.getElementById("voltage-chart"), [[0, null, null]], {
		labels: ["Hora", "Fase A", "Fase B"],
		xValueParser: function(x) {return x;},
		drawPoints: false,
		labelsDiv: document.getElementById("voltage-chart-labels"),
		legend: "always",
		valueRange: [(127 * 0.75), (127 * 1.25)],
		axes: {
			y: {
				axisLabelFormatter: function(x) { return x + " V"; }
			}
		}
	});
	
	Dygraph.synchronize([window.smceePowerChart, window.smceeVoltageChart], {
		zoom: true,
		selection: true,
		range: false
	});
}

function initPage() {
	userInfoFetch(function() {navbarPopulateItems("main-menu");});
	
	fetchApplianceList();
	fetchPowerData(12 * 3600);
}

function showVoltageChart() {
	document.getElementById("button-show-voltage-container").classList.add("is-loading");
	
	fetchVoltageData();
}

function changeDisplayEvents() {
	if(this.checked && typeof window.powerChartAnnotations == "object")
		window.smceePowerChart.setAnnotations(window.powerChartAnnotations);
	else
		window.smceePowerChart.setAnnotations([]);
}

function fetchPowerData(secondQty) {
	var xhrFetchData = new XMLHttpRequest();
	
	xhrFetchData.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			var lastTimestamp = null;
			
			window.smceePowerData = [];
			
			if(responseObject.length < 1)
				return;
			
			window.smceeDataStartTimestamp = responseObject[0][0];
			window.smceeDataEndTimestamp = responseObject[responseObject.length - 1][0];
			
			window.smceePowerData.push([new Date((smceeDataEndTimestamp - secondQty - 1) * 1000), null]);
			
			for(let pdItem of responseObject) {
				if(lastTimestamp !== null && pdItem[0] - lastTimestamp > 1)
					window.smceePowerData.push([new Date((lastTimestamp + 1) * 1000), null]);
				
				window.smceePowerData.push([new Date(pdItem[0] * 1000), pdItem[1]]);
				
				lastTimestamp = pdItem[0];
			}
			
			window.smceePowerData.push([new Date((lastTimestamp + 1) * 1000), null]);
			
			window.smceePowerChart.updateOptions({'file' : window.smceePowerData});
			
			fetchPowerEvents(secondQty);
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch power data. Status: " + this.status);
		}
	}
	
	xhrFetchData.open("GET", window.smceeApiUrlBase + "power?type=pt&last=" + secondQty);
	
	xhrFetchData.timeout = 2000;
	
	xhrFetchData.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrFetchData.send();
}

function fetchVoltageData() {
	var xhrFetchData = new XMLHttpRequest();
	
	xhrFetchData.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			var lastTimestamp = null;
			
			window.smceeVoltageData = [];
			
			if(responseObject.length < 1)
				return;
			
			document.getElementById("button-show-voltage-container").parentNode.classList.add("is-hidden");
			document.getElementById("voltage-chart-container").classList.remove("is-hidden");
			
			window.smceeVoltageData.push([new Date((window.smceeDataEndTimestamp - 1) * 1000), null, null]);
			
			for(let pdItem of responseObject) {
				if(lastTimestamp !== null && pdItem[0] - lastTimestamp > 1)
					window.smceeVoltageData.push([new Date((lastTimestamp + 1) * 1000), null, null]);
				
				window.smceeVoltageData.push([new Date(pdItem[0] * 1000), pdItem[2], pdItem[3]]);
				
				lastTimestamp = pdItem[0];
			}
			
			window.smceeVoltageData.push([new Date((lastTimestamp + 1) * 1000), null, null]);
			
			window.smceeVoltageChart.updateOptions({'file' : window.smceeVoltageData});
			window.smceeVoltageChart.resize();
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch power data. Status: " + this.status);
		}
	}
	
	xhrFetchData.open("GET", window.smceeApiUrlBase + "power?type=ptv&start=" + window.smceeDataStartTimestamp + "&end=" + window.smceeDataEndTimestamp);
	
	xhrFetchData.timeout = 2000;
	
	xhrFetchData.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrFetchData.send();
}

function fetchPowerEvents(secondQty) {
	var xhrPowerEvents = new XMLHttpRequest();
	
	xhrPowerEvents.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			console.info(responseObject.length + " power events.");
			
			window.smceeLoadEvents = responseObject;
			
			if(typeof window.smceeApplianceList == "object")
				generateAnnotations();
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch power events. Status: " + this.status);
		}
	}
	
	xhrPowerEvents.open("GET", window.smceeApiUrlBase + "power/events?last=" + secondQty);
	
	xhrPowerEvents.timeout = 2000;
	
	xhrPowerEvents.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrPowerEvents.send();
}

function generateAnnotations() {
	window.powerChartAnnotations = [];
	
	if(typeof window.smceeLoadEvents != "object" || typeof window.smceeApplianceList != "object")
		return;
	
	for(eventItem of window.smceeLoadEvents) {
		window.powerChartAnnotations.push({
			series: "Potência",
			x: eventItem.timestamp * 1000,
			shortText: (eventItem.delta_pt > 0) ? "L" : "D",
			text: "d: " + eventItem.duration + " secs\ndP: " + eventItem.delta_pt.toFixed(1) + " W (" + eventItem.delta_p[0].toFixed(1) + " | " + eventItem.delta_p[1].toFixed(1) + ")\nPpk: " + eventItem.peak_pt.toFixed(1) + " W\ndQ: " + eventItem.delta_q[0].toFixed(1) + " | " + eventItem.delta_q[1].toFixed(1) + " VAr",
			tickColor: "gray",
			tickHeight: 10,
		});
	}
	
	document.getElementById("checkbox-display-events").disabled = false;
}

function fetchApplianceList() {
	var xhrApplianceList = new XMLHttpRequest();
	
	xhrApplianceList.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			window.smceeApplianceList = new Map();
			
			for(applianceItem of responseObject)
				window.smceeApplianceList.set(applianceItem.id, applianceItem);
			
			document.getElementById("button-show-voltage-container").classList.remove("is-loading");
			
			if(typeof window.smceeLoadEvents == "object")
				generateAnnotations()
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch appliance list. Status: " + this.status);
		}
	}
	
	xhrApplianceList.open("GET", window.smceeApiUrlBase + "appliances");
	
	xhrApplianceList.timeout = 2000;
	
	xhrApplianceList.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrApplianceList.send();
}
