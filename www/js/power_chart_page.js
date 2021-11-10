window.onload = function () {
	authCheckAccessKey(initPage, redirectToLogin);
	addEventsNavbarBurgers();
	
	document.getElementById("checkbox-display-events").checked = false;
	
	document.getElementById("checkbox-separate-phases").onchange = updateChart;
	document.getElementById("checkbox-display-voltage").onchange = updateChart;
	document.getElementById("checkbox-display-events").onchange = toggleDisplayEvents;
	
	window.smceeHighlightedEvents = new Set();
	
	window.smceePowerChart = new Dygraph(document.getElementById("power-chart"), [[0, null, null, null, null, null]], {
		labels: ["Hora", "Potência", "Potência A", "Potência B", "Tensão A", "Tensão B"],
		colors: ["#172769", "#193CC8", "#1E6EAC", "#AC581E", "#AC281E"],
		xValueParser: function(x) {return x;},
		drawPoints: false,
		legend: "always",
		series: {
			"Tensão A": {
				axis: "y2"
			},
			"Tensão B": {
				axis: "y2"
			}
		},
		axes: {
			y: {
				includeZero: true,
				gridLineColor: "#6797D4",
				axisLabelFormatter: function(x) { return x + " W"; }
			},
			y2: {
				drawAxis: true,
				independentTicks: true,
				drawGrid: true,
				gridLineColor: "#D36A4E",
				axisLabelFormatter: function(x) { return x.toFixed(1) + " V"; },
			}
		},
		annotationClickHandler: annotationClickHandler
	});
}

function initPage() {
	userInfoFetch(function() {navbarPopulateItems("main-menu");});
	
	fetchPVData(12 * 3600);
}

function toggleDisplayEvents() {
	if(this.checked)
		updateAnnotations();
	else
		window.smceePowerChart.setAnnotations([]);
}

function updateChart() {
	var separatePhases = document.getElementById("checkbox-separate-phases").checked;
	var displayVoltage = document.getElementById("checkbox-display-voltage").checked;
	
	if(separatePhases) {
		document.getElementById("checkbox-display-events").checked = false;
		window.smceePowerChart.setAnnotations([]);
	}
	
	document.getElementById("checkbox-display-events").disabled = separatePhases;
	
	window.smceePowerChart.setVisibility([!separatePhases, separatePhases, separatePhases, displayVoltage, displayVoltage]);
}

function fetchPVData(secondQty) {
	var xhrFetchData = new XMLHttpRequest();
	
	xhrFetchData.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			var startTimestamp, endTimestamp;
			var chartDataFile = [];
			var lastTimestamp = null;
			
			window.smceePVData = responseObject;
			
			startTimestamp = responseObject[0][0];
			endTimestamp = responseObject[responseObject.length - 1][0];
			
			fetchPowerEvents(startTimestamp, endTimestamp);
			
			startTimestamp = window.smceePVData[0][0];
			endTimestamp = window.smceePVData[window.smceePVData.length - 1][0];
			
			chartDataFile.push([new Date((endTimestamp - secondQty - 1) * 1000), null, null, null, null, null]);
			
			for(let pdItem of window.smceePVData) {
				if(lastTimestamp !== null && pdItem[0] - lastTimestamp > 1)
					chartDataFile.push([new Date((lastTimestamp + 1) * 1000), null, null, null, null, null]);
				
				chartDataFile.push([new Date(pdItem[0] * 1000), (pdItem[1] + pdItem[2]), pdItem[1], pdItem[2], pdItem[3], pdItem[4]]);
				
				lastTimestamp = pdItem[0];
			}
			
			chartDataFile.push([new Date((lastTimestamp + 1) * 1000), null, null, null, null, null]);
			
			window.smceePowerChart.updateOptions({
				'file': chartDataFile,
			});
			
			updateChart();
			
			document.getElementById("checkbox-separate-phases").disabled = false;
			document.getElementById("checkbox-display-voltage").disabled = false;
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch power data. Status: " + this.status);
		}
	}
	
	xhrFetchData.open("GET", window.smceeApiUrlBase + "power?type=pv&last=" + secondQty);
	
	xhrFetchData.timeout = 2000;
	
	xhrFetchData.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrFetchData.send();
}

function fetchPowerEvents(startTimestamp, endTimestamp) {
	var xhrPowerEvents = new XMLHttpRequest();
	
	xhrPowerEvents.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			console.info(responseObject.length + " power events.");
			
			window.smceeLoadEvents = new Map();
			
			for(eventItem of responseObject)
				window.smceeLoadEvents.set(eventItem.timestamp, eventItem);
			
			fetchApplianceList(function() { document.getElementById("checkbox-display-events").disabled = false; });
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch power events. Status: " + this.status);
		}
	}
	
	xhrPowerEvents.open("GET", window.smceeApiUrlBase + "power/events?start=" + startTimestamp + "&end=" + endTimestamp);
	
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
