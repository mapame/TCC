window.onload = function () {
	authCheckAccessKey(initPage, redirectToLogin);
	addEventsNavbarBurgers();
	
	document.getElementById("checkbox-display-events").checked = false;
	
	document.getElementById("button-show-voltage-container").onclick = showVoltageChart;
	document.getElementById("checkbox-display-events").onchange = changeDisplayEvents;
	
	window.smceeHighlightedEvents = new Set();
	
	window.smceePowerChart = new Dygraph(document.getElementById("power-chart"), [[0, null]], {
		labels: ["Hora", "Potência"],
		xValueParser: function(x) {return x;},
		drawPoints: false,
		legend: "always",
		includeZero: true,
		axes: {
			y: {
				axisLabelFormatter: function(x) { return x + " W"; }
			}
		},
		annotationClickHandler: annotationClickHandler
	});
	
	window.smceeVoltageChart = new Dygraph(document.getElementById("voltage-chart"), [[0, null, null]], {
		labels: ["Hora", "Fase A", "Fase B"],
		xValueParser: function(x) {return x;},
		drawPoints: false,
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
	if(this.checked)
		updateAnnotations();
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
			
			window.smceeLoadEvents = new Map();
			
			for(eventItem of responseObject)
				window.smceeLoadEvents.set(eventItem.timestamp, eventItem);
			
			if(typeof window.smceeApplianceList == "object")
				document.getElementById("checkbox-display-events").disabled = false;
			
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

function updateAnnotations() {
	var annotations = [];
	
	if(typeof window.smceeLoadEvents != "object" || typeof window.smceeApplianceList != "object")
		return;
	
	for(eventItem of window.smceeLoadEvents.values()) {
		let highlighted = window.smceeHighlightedEvents.has(eventItem.timestamp);
		let annotation = {
			series: "Potência",
			x: eventItem.timestamp * 1000,
			shortText: eventItem.state,
			text: eventItem.raw_delta_pt.toFixed(1) + " W em " + eventItem.duration + " seg\n",
			cssClass: highlighted ? "dygraph-highlighted-annotation" : "",
			tickHeight: 20,
			tickColor: highlighted ? "red" : "black",
			tickWidth: 1,
		};
		
		annotation.text += "\ndP: " + eventItem.delta_pt.toFixed(1) + " W (" + eventItem.delta_p[0].toFixed(1) + " | " + eventItem.delta_p[1].toFixed(1) + ")";
		annotation.text += "\nPpk: " + eventItem.peak_pt.toFixed(1) + " W";
		annotation.text += "\ndS(VA): " + eventItem.delta_s[0].toFixed(1) + " | " + eventItem.delta_s[1].toFixed(1);
		annotation.text += "\ndQ(VAr): " + eventItem.delta_q[0].toFixed(1) + " | " + eventItem.delta_q[1].toFixed(1);
		
		if(eventItem.time_gap > 0)
			annotation.text += "\nLacuna: " + eventItem.time_gap + " s";
		
		if(eventItem.state >= 3)
			annotation.text += "\n\nAparelho: " + window.smceeApplianceList.get(eventItem.pair_appliance_id).name + " (" + eventItem.pair_score + ")";
		
		if(eventItem.state >= 2) {
			annotation.text += "\n\nOutlier Score: " + eventItem.outlier_score.toFixed(2) + "\n";
			
			for(let idx = 0; idx < 3; idx++)
				if(eventItem.possible_appliances[idx] > 0)
					annotation.text += "\n" + (idx + 1) + "- " + window.smceeApplianceList.get(eventItem.possible_appliances[idx]).name;
		}
		
		annotations.push(annotation);
	}
	
	window.smceePowerChart.setAnnotations(annotations);
}

function annotationClickHandler(annotation, point, dygraph, event) {
	var loadEvent = window.smceeLoadEvents.get(annotation.x / 1000);
	var pairLoadEvent;
	
	if(typeof loadEvent == "undefined" || loadEvent.state != 3)
		return;
	
	pairLoadEvent = window.smceeLoadEvents.get(loadEvent.pair_timestamp);
	
	if(typeof pairLoadEvent == "undefined")
		return;
	
	if(window.smceeHighlightedEvents.has(loadEvent.timestamp) || window.smceeHighlightedEvents.has(pairLoadEvent.timestamp)) {
		smceeHighlightedEvents.clear();
	} else {
		smceeHighlightedEvents.clear();
		window.smceeHighlightedEvents.add(loadEvent.timestamp);
		window.smceeHighlightedEvents.add(pairLoadEvent.timestamp);
	}
	
	updateAnnotations();
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
				document.getElementById("checkbox-display-events").disabled = false;
			
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
