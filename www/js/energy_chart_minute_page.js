window.onload = function() {
	authCheckAccessKey(initPage, redirectToLogin);
	addEventsNavbarBurgers();
}

function initPage() {
	var dateMax = new Date();
	var dateMin = new Date(dateMax - 2 * 30 * 24 * 3600 * 1000);
	
	userInfoFetch(function() {navbarPopulateItems("main-menu");});
	
	window.smceeEnergyData = {
		date: "",
		data: []
	};
	
	document.getElementById("date-input").valueAsDate = dateMax;
	document.getElementById("date-input").min = dateMin.getFullYear() + "-" + (dateMin.getMonth() + 1).toString().padStart(2, "0") + "-" +  dateMin.getDate().toString().padStart(2, "0");
	document.getElementById("date-input").max = dateMax.getFullYear() + "-" + (dateMax.getMonth() + 1).toString().padStart(2, "0") + "-" +  dateMax.getDate().toString().padStart(2, "0");
	
	document.getElementById("disaggregation-checkbox").checked = false;
	
	document.getElementById("date-input").onchange = updateChart;
	document.getElementById("disaggregation-checkbox").onchange = updateChart;
	document.getElementById("reactive-checkbox").onchange = updateChart;
	
	window.smceeEnergyChart = new Dygraph(document.getElementById("minute-energy-chart"), [[0, null, null]], {
		labels: ["Hora", "Energia", "Energia Reativa"],
		xValueParser: function(x) {return x;},
		drawPoints: false,
		labelsDiv: document.getElementById("chart-labels"),
		legend: "always",
		includeZero: true,
		axes: {
			y: {
				axisLabelWidth: 20,
				valueFormatter: function(y, opts, series_name) {
									return y.toFixed(1) + (series_name == 'Energia' ? " Wh" : " VArh");
								}
			},
		},
	});
	
	updateChart();
}

function updateChart() {
	var selectedDate = document.getElementById('date-input').value;
	
	if(window.smceeEnergyData.date != selectedDate) {
		window.smceeEnergyData.date != selectedDate;
		fetchEnergyData();
	}
	
	if(document.getElementById("disaggregation-checkbox").checked) {
		document.getElementById("reactive-checkbox").checked = false;
		document.getElementById("reactive-checkbox").disabled = true;
	} else {
		window.smceeEnergyChart.setVisibility(0, true);
		window.smceeEnergyChart.setVisibility(1, document.getElementById("reactive-checkbox").checked);
	}
}

function fetchEnergyData() {
	var selectedDate = document.getElementById('date-input').value.split("-");
	var startTimestamp = (new Date(Number(selectedDate[0]), Number(selectedDate[1]) - 1, Number(selectedDate[2]))) / 1000;
	var endTimestamp = startTimestamp + 24 * 3600 - 1;
	var xhrEnergyData = new XMLHttpRequest();
	
	xhrEnergyData.onload = function() {
		if(this.status == 200) {
			let responseObj = JSON.parse(this.responseText);
			var lastTimestamp = null;
			
			window.smceeEnergyData.data = [];
			
			window.smceeEnergyData.data.push([new Date((startTimestamp - 1) * 1000), null, null]);
			
			for(const element of responseObj) {
				if(element.second_count < 15)
					continue;
				
				if(element.timestamp - lastTimestamp > 60 && lastTimestamp !== null)
					window.smceeEnergyData.data.push([new Date((element.timestamp - 1) * 1000), null, null]);
				
				window.smceeEnergyData.data.push([new Date(element.timestamp * 1000), element.active * 1000, element.reactive * 1000])
				
				lastTimestamp = element.timestamp;
			}
			
			window.smceeEnergyData.data.push([new Date((endTimestamp + 1) * 1000), null, null]);
			
			window.smceeEnergyChart.updateOptions({'file' : window.smceeEnergyData.data});
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch energy data. Status: " + this.status);
		}
	}
	
	xhrEnergyData.open("GET", window.smceeApiUrlBase + "energy/minutes?start=" + startTimestamp + "&end=" + endTimestamp);
	
	xhrEnergyData.timeout = 2000;
	
	xhrEnergyData.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrEnergyData.send();
}
