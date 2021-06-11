window.onload = function() {
	var voltageGaugeOpts;
	
	authCheckAccessKey(initPage, redirectToLogin);
	addEventsNavbarBurgers();
	
	window.dashboardPowerGauge = Gauge(document.getElementById("power-gauge"), {
			min: 0,
			max: 6000,
			dialStartAngle: 180,
			dialEndAngle: 0,
			value: 0,
			viewBox: "0 0 100 57",
			label: function(value) {
				return Math.round(value) + " W";
			},
			color: function(value) {
				if(value < 500) {
					return "#5ee432";
				} else if(value < 1000) {
					return "#fffa50";
				} else if(value < 2000) {
					return "#f7aa38";
				} else {
					return "#fa1c2f";
				}
			}
		}
	);
	
	voltageGaugeOpts = {
		min: (127 * 0.75),
		max: (127 * 1.25),
		dialStartAngle: 180,
		dialEndAngle: 0,
		value: 0,
		needleIndicator: true,
		needleAngle: 4,
		viewBox: "0 0 100 57",
		label: function(value) {
			return value.toFixed(1) + " V";
		},
		color: function(value) {
			if(value < (127 * 0.8) || value > (127 * 1.2)) {
				return "#fa1c2f";
			} else if(value < (127 * 0.9) || value > (127 * 1.1)) {
				return "#f7aa38";
			} else {
				return "#5ee432";
			}
		}
	};
	
	window.dashboardVoltageGaugeA = Gauge(document.getElementById("voltage-a-gauge"), voltageGaugeOpts);
	window.dashboardVoltageGaugeB = Gauge(document.getElementById("voltage-b-gauge"), voltageGaugeOpts);
	
	window.dashboardPowerChart = new Dygraph(document.getElementById("power-chart"), [[0, null]], {
			labels: ["Hora", "Potência"],
			interactionModel: {},
			xValueParser: function(x) {return x;},
			drawPoints: false,
			legend: "never",
			includeZero: true,
			axes: {
				y: {
					axisLabelFormatter: function(x) { return x + " W"; }
				}
			}
		}
	);
}

function initPage() {
	userInfoFetch(function() {navbarPopulateItems("main-menu");});
	
	fetchPastPowerData();
	dashboardFetchData();
}

function dashboardPowerGraphAddData(timestamp, power) {
	if(typeof window.dashboardPowerData == "undefined")
		window.dashboardPowerData = [];
	
	if(window.dashboardPowerData.length > 0) {
		let lastTimestamp = window.dashboardPowerData[window.dashboardPowerData.length - 1][0] / 1000;
		
		if(timestamp <= lastTimestamp)
			return;
		
		for(let missingTimestamp = lastTimestamp + 1; missingTimestamp < timestamp; missingTimestamp++)
			window.dashboardPowerData.push([new Date(missingTimestamp * 1000), null]);
	}
	
	window.dashboardPowerData.push([
		new Date(timestamp * 1000),
		power
	]);
	
	if(window.dashboardPowerData.length > 3600)
		window.dashboardPowerData = window.dashboardPowerData.slice(window.dashboardPowerData.length - 3600);
}

function fetchPastPowerData() {
	var accessKey = localStorage.getItem("access_key");
	var xhrHistoricData = new XMLHttpRequest();
	
	xhrHistoricData.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			for(let pdEntry of responseObject)
				dashboardPowerGraphAddData(pdEntry[0], pdEntry[1]);
			
			window.dashboardPowerChart.updateOptions({'file' : window.dashboardPowerData});
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch historic power data. Status: " + this.status);
		}
	}
	
	xhrHistoricData.open("GET", window.smceeApiUrlBase + "power?type=pt&last=3600");
	
	xhrHistoricData.timeout = 2000;
	
	xhrHistoricData.setRequestHeader("Authorization", "Bearer " + accessKey);
	
	xhrHistoricData.send();
}

function dashboardFetchData() {
	var accessKey = localStorage.getItem("access_key");
	var xhrDashboardData;
	
	window.loggedUserInfo = null;
	
	if(accessKey === null)
		return;
	
	xhrDashboardData = new XMLHttpRequest()
	
	xhrDashboardData.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			if(typeof responseObject === "object") {
				let daysInMonth = new Date(responseObject.today.date.year, responseObject.today.date.month, 0).getDate();
				let monthName = new Date(responseObject.today.date.year, responseObject.today.date.month - 1).toLocaleString("pt-br", { month: "long" });
				
				for(let currentMonthLabelElement of document.getElementsByClassName("current-month-label"))
					currentMonthLabelElement.innerText = monthName;
				
				document.getElementById("energy-today-value").innerText = responseObject.today.energy.toFixed(1);
				document.getElementById("energy-today-cost").innerText = responseObject.today.cost.toFixed(2);
				
				document.getElementById("energy-this-month-value").innerText = responseObject.thismonth.energy.toFixed(1);
				document.getElementById("energy-this-month-cost").innerText = responseObject.thismonth.cost.toFixed(2);
				
				document.getElementById("energy-dailyavg-value").innerText = responseObject.dailyavg.energy.toFixed(1);
				document.getElementById("energy-dailyavg-cost").innerText = responseObject.dailyavg.cost.toFixed(2);
				
				document.getElementById("energy-month-estimate-value").innerText = (responseObject.thismonth.energy - responseObject.today.energy + responseObject.dailyavg.energy * (daysInMonth - responseObject.today.date.day) + Math.max(responseObject.dailyavg.energy, responseObject.today.energy)).toFixed(1);
				document.getElementById("energy-month-estimate-cost").innerText = (responseObject.thismonth.cost - responseObject.today.cost + responseObject.dailyavg.cost * (daysInMonth - responseObject.today.date.day) + Math.max(responseObject.dailyavg.cost, responseObject.today.cost)).toFixed(2);
				
				if(typeof responseObject.power == "object" && responseObject.power.length > 1) {
					let lastPowerData = responseObject.power[responseObject.power.length - 1];
					
					if(typeof window.dashboardPowerData == "object") {
						for(pdItem of responseObject.power)
							dashboardPowerGraphAddData(pdItem.timestamp, pdItem.p1 + pdItem.p2);
						
						window.dashboardPowerChart.updateOptions({'file' : window.dashboardPowerData});
					}
					
					window.dashboardPowerGauge.setValueAnimated(lastPowerData.p1 + lastPowerData.p2, 0.2);
					
					window.dashboardVoltageGaugeA.setValueAnimated(lastPowerData.v1, 0.2);
					window.dashboardVoltageGaugeB.setValueAnimated(lastPowerData.v2, 0.2);
					
					document.getElementById("power-a-value").innerText = lastPowerData.p1.toFixed(1);
					document.getElementById("power-b-value").innerText = lastPowerData.p2.toFixed(1);
					
					document.getElementById("hourly-cost").innerText = ((lastPowerData.p1 + lastPowerData.p2) * responseObject.energy_rate / 1000).toFixed(2);
				}
			}
			
			setTimeout(dashboardFetchData, 2000);
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch dashboard data. Status: " + this.status);
			
			setTimeout(dashboardFetchData, 1000);
		}
	};
	
	xhrDashboardData.open("GET", window.smceeApiUrlBase + "dashboard");
	
	xhrDashboardData.timeout = 2000;
	
	xhrDashboardData.setRequestHeader("Authorization", "Bearer " + accessKey);
	
	xhrDashboardData.send();
}
