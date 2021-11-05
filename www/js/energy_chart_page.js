window.onload = function() {
	authCheckAccessKey(initPage, redirectToLogin);
	addEventsNavbarBurgers();
}

function initPage() {
	userInfoFetch(function() {navbarPopulateItems("main-menu");});
	
	fetchEnergyDateInfo();
	
	fetchApplianceList(function() {document.getElementById("disaggregated-energy-checkbox").disabled = false;});
	
	window.smceeEnergyData = {};
	window.smceeEnergyData.type = "";
	window.smceeEnergyData.energyData = {};
	window.smceeEnergyData.energyComparisonData = {};
	window.smceeEnergyData.disaggregatedEnergyData = {};
	
	document.getElementById("disaggregated-energy-checkbox").checked = false;
	
	document.getElementById("chart-type-select").onchange = changeChartType;
	document.getElementById("comparison-checkbox").onchange = updateChart;
	document.getElementById("cost-checkbox").onchange = updateChart;
	document.getElementById("coverage-checkbox").onchange = updateChart;
	document.getElementById("disaggregated-energy-checkbox").onchange = updateChart;
	
	document.getElementById("date-day-input").onchange = updateChart;
	document.getElementById("date-month-select").onchange = updateChart;
	document.getElementById("date-year-select").onchange = updateChart;
	
	document.getElementById("comparison-date-day-input").onchange = updateChart;
	document.getElementById("comparison-date-month-select").onchange = updateChart;
	document.getElementById("comparison-date-year-select").onchange = updateChart;
	
	window.smceeEnergyBarChart = new Chart(document.getElementById('energy-bar-chart-canvas'), {
		type: 'bar',
		data: {
			datasets: []
		},
		options: {
			stacked: false,
			responsive: true,
			maintainAspectRatio: false,
			animation: {
				duration: 0
			},
			tooltips: {
				mode: 'index',
				intersect: false
			},
			scales: {
				x: {
					display: true,
					title: {
						display: true,
						text: 'Hora'
					}
				},
				y: {
					display: true,
					position: 'left',
					title: {
						display: true,
						text: 'Energia (kWh)'
					}
				},
				y1: {
					display: false,
					position: 'right',
					title: {
						display: true,
						text: 'Cobertura (%)'
					},
					grid: {
					  drawOnChartArea: false,
					}
				}
			},
			plugins: {
				legend: {
					display: true,
					onClick: handleLegendClick,
				},
				tooltip: {
					filter: (element => element.raw > 0),
				}
			}
		}
	});
	
	window.smceeEnergyPieChart = new Chart(document.getElementById('energy-pie-chart-canvas'), {
		type: 'pie',
		data: {
			datasets: []
		},
		options: {
			responsive: true,
			maintainAspectRatio: false,
			animation: {
				duration: 0
			},
			plugins: {
				legend: {
					display: false,
				}
			}
		}
	});
}

function daysInMonth(month, year) {
    return new Date(year, month, 0).getDate();
}

function handleLegendClick(evt, item, legend) {
	if(typeof window.smceeHighlightedDataset == "string" && item.text === window.smceeHighlightedDataset) {
		legend.chart.data.datasets.forEach((dataset) => {
			dataset.backgroundColor = (dataset.backgroundColor.length === 9) ? dataset.backgroundColor.slice(0, -2) : dataset.backgroundColor;
		});
		
		window.smceeHighlightedDataset = null;
	} else {
		legend.chart.data.datasets.forEach((dataset) => {
			if(dataset.label === item.text) {
				dataset.backgroundColor = (dataset.backgroundColor.length === 9) ? dataset.backgroundColor.slice(0, -2) : dataset.backgroundColor;
			} else {
				dataset.backgroundColor = dataset.backgroundColor + ((dataset.backgroundColor.length === 9) ? '' : '46');
			}
		});

		window.smceeHighlightedDataset = item.text;
	}

	legend.chart.update();
}

function fillDateInputs(yearArray, lastEnergyMinuteTimestamp) {
	var monthNames = ["Janeiro", "Fevereiro", "Março", "Abril", "Maio", "Junho", "Julho", "Agosto", "Setembro", "Outubro", "Novembro", "Dezembro"];
	var lastEnergyMinute = new Date(lastEnergyMinuteTimestamp * 1000);
	
	var yearSelect = document.getElementById("date-year-select");
	var yearComparisonSelect = document.getElementById("comparison-date-year-select");
	var monthSelect = document.getElementById("date-month-select");
	var monthComparisonSelect = document.getElementById("comparison-date-month-select");
	var dayInput = document.getElementById("date-day-input");
	var ComparisonDayInput = document.getElementById("comparison-date-day-input");
	var newSelectOption;
	
	if(typeof yearArray != "object" || typeof lastEnergyMinuteTimestamp != "number" || yearArray.length < 1)
		return;
	
	for(const year of yearArray) {
		newSelectOption = document.createElement("option");
		newSelectOption.innerText = year.year;
		newSelectOption.value = year.year;
		
		yearSelect.appendChild(newSelectOption);
		yearComparisonSelect.appendChild(newSelectOption.cloneNode(true));
		
		for(const month of year.months) {
			newSelectOption = document.createElement("option");
			newSelectOption.innerText = monthNames[month - 1] + " " + year.year;
			newSelectOption.value = month + "-" + year.year;
			
			monthSelect.appendChild(newSelectOption);
			monthComparisonSelect.appendChild(newSelectOption.cloneNode(true));
		}
	}
	
	dayInput.min = yearArray[0].year + "-" + yearArray[0].months[0].toString().padStart(2, "0") + "-01";
	dayInput.max = lastEnergyMinute.getFullYear() + "-" + (lastEnergyMinute.getMonth() + 1).toString().padStart(2, "0") + "-" +  lastEnergyMinute.getDate().toString().padStart(2, "0");
	
	ComparisonDayInput.min = dayInput.min;
	ComparisonDayInput.max = dayInput.max;
	
	yearSelect.selectedIndex = yearSelect.length - 1;
	yearComparisonSelect.selectedIndex = yearComparisonSelect.length - 1;
	
	monthSelect.selectedIndex = monthSelect.length - 1;
	monthComparisonSelect.selectedIndex = monthComparisonSelect.length - 1;
	
	dayInput.value = dayInput.max;
	ComparisonDayInput.value = ComparisonDayInput.max;
	
	yearSelect.disabled = false;
	monthSelect.disabled = false;
	dayInput.disabled = false;
	ComparisonDayInput.disabled = false;
}

function changeChartType() {
	let selectedType = document.getElementById("chart-type-select").value;
	
	let yearSelect = document.getElementById("date-year-select");
	let yearComparisonSelect = document.getElementById("comparison-date-year-select");
	let monthSelect = document.getElementById("date-month-select");
	let monthComparisonSelect = document.getElementById("comparison-date-month-select");
	let dayInput = document.getElementById("date-day-input");
	let dayComparisonInput = document.getElementById("comparison-date-day-input");
	
	if(selectedType == "hours") {
		yearSelect.parentNode.classList.add("is-hidden");
		yearComparisonSelect.parentNode.classList.add("is-hidden");
		
		monthSelect.parentNode.classList.add("is-hidden");
		monthComparisonSelect.parentNode.classList.add("is-hidden");
		
		dayInput.classList.remove("is-hidden");
		dayComparisonInput.classList.remove("is-hidden");
		
	} else if(selectedType == "days") {
		yearSelect.parentNode.classList.add("is-hidden");
		yearComparisonSelect.parentNode.classList.add("is-hidden");
		
		monthSelect.parentNode.classList.remove("is-hidden");
		monthComparisonSelect.parentNode.classList.remove("is-hidden");
		
		dayInput.classList.add("is-hidden");
		dayComparisonInput.classList.add("is-hidden");
		
	} else {
		yearSelect.parentNode.classList.remove("is-hidden");
		yearComparisonSelect.parentNode.classList.remove("is-hidden");
		
		monthSelect.parentNode.classList.add("is-hidden");
		monthComparisonSelect.parentNode.classList.add("is-hidden");
		
		dayInput.classList.add("is-hidden");
		dayComparisonInput.classList.add("is-hidden");
	}
	
	window.smceeEnergyData.energyData.date = "";
	window.smceeEnergyData.energyComparisonData.date = "";
	window.smceeEnergyData.disaggregatedEnergyData.date = "";
	
	updateChart();
}

function fetchEnergyDateInfo() {
	var xhrEnergyDates = new XMLHttpRequest();
	
	xhrEnergyDates.onload = function() {
		if(this.status == 200) {
			var responseObj = JSON.parse(this.responseText);
			
			if(responseObj.length < 1 || typeof responseObj.years != "object" || typeof responseObj.minute_min_timestamp != "number" || typeof responseObj.minute_max_timestamp != "number")
				return;
			
			fillDateInputs(responseObj.years, responseObj.minute_max_timestamp);
			
			changeChartType();
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch energy dates. Status: " + this.status);
		}
	}
	
	xhrEnergyDates.open("GET", window.smceeApiUrlBase + "energy");
	
	xhrEnergyDates.timeout = 2000;
	
	xhrEnergyDates.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrEnergyDates.send();
}

function formatEnergyData(receivedEnergyData, energyDataType, comparisonData) {
	var energyTarget = comparisonData ? window.smceeEnergyData.energyComparisonData : window.smceeEnergyData.energyData;
	
	energyTarget.energy = [];
	energyTarget.cost = [];
	energyTarget.coverage = [];
	
	for(const element of receivedEnergyData) {
		let idx = (energyDataType == "months" ? element.month - 1 : (energyDataType == "days" ? element.day - 1 : element.hour));
		let totalSeconds = 3600 * (energyDataType == "months" ? daysInMonth(element.month, Number(energyTarget.date)) * 24 : (energyDataType == "days" ? 24 : 1));
		
		energyTarget.energy[idx] = element.active.toFixed(2);
		energyTarget.cost[idx] = element.cost.toFixed(2);
		energyTarget.coverage[idx] = ((element.second_count / totalSeconds) * 100).toFixed(1);
	}
}

function formatDisaggregatedEnergyData(receivedEnergyData, energyDataType, applianceQty) {
	var numRows = 0;
	var totalEnergy = 0;
	var appliances = new Set();
	
	if(applianceQty <= 0)
		return;
	
	window.smceeEnergyData.disaggregatedEnergyData.totalStandbyEnergy = 0;
	window.smceeEnergyData.disaggregatedEnergyData.totalUnknownEnergy = 0;
	
	window.smceeEnergyData.disaggregatedEnergyData.insignificantAppliancesTotalEnergy = 0;
	window.smceeEnergyData.disaggregatedEnergyData.insignificantAppliancesTotalCost = 0;
	
	window.smceeEnergyData.disaggregatedEnergyData.applianceTotalEnergy = new Array(applianceQty).fill(0);
	window.smceeEnergyData.disaggregatedEnergyData.applianceTotalCost = new Array(applianceQty).fill(0);
	
	for(const element of receivedEnergyData) {
		let applianceEnergyTotal = 0;
		
		for(let applianceId = 0; applianceId < applianceQty; applianceId++) {
			if(typeof element.appliance_energy[applianceId] == "undefined")
				break;
			
			if(element.appliance_energy[applianceId] == null)
				continue;
			
			appliances.add(applianceId);
			
			window.smceeEnergyData.disaggregatedEnergyData.applianceTotalEnergy[applianceId] += element.appliance_energy[applianceId];
			window.smceeEnergyData.disaggregatedEnergyData.applianceTotalCost[applianceId] += element.appliance_cost[applianceId];
			
			applianceEnergyTotal += element.appliance_energy[applianceId];
		}
		
		window.smceeEnergyData.disaggregatedEnergyData.totalStandbyEnergy += element.standby_energy;
		window.smceeEnergyData.disaggregatedEnergyData.totalUnknownEnergy += Math.max(0, element.total_energy - applianceEnergyTotal);
		
		totalEnergy += element.total_energy;
		
		numRows = Math.max(numRows, (energyDataType == "months" ? element.month : (energyDataType == "days" ? element.day : element.hour + 1)));
	}
	
	window.smceeEnergyData.disaggregatedEnergyData.standbyEnergy = new Array(numRows).fill(0);
	window.smceeEnergyData.disaggregatedEnergyData.unknownEnergy = new Array(numRows).fill(0);
	
	window.smceeEnergyData.disaggregatedEnergyData.insignificantAppliancesEnergy = new Array(numRows).fill(0);
	window.smceeEnergyData.disaggregatedEnergyData.insignificantAppliancesCost = new Array(numRows).fill(0);
	
	window.smceeEnergyData.disaggregatedEnergyData.applianceEnergy = new Map();
	window.smceeEnergyData.disaggregatedEnergyData.applianceCost = new Map();
	
	for(let applianceId = 0; applianceId < applianceQty; applianceId++) {
		let applianceIdTotalEnergy = window.smceeEnergyData.disaggregatedEnergyData.applianceTotalEnergy[applianceId];
		
		if(applianceIdTotalEnergy <= 0)
			continue;
		
		if(appliances.size < 12 || applianceIdTotalEnergy >= totalEnergy * 0.005) {
			window.smceeEnergyData.disaggregatedEnergyData.applianceEnergy.set(applianceId, new Array(numRows).fill(0));
			window.smceeEnergyData.disaggregatedEnergyData.applianceCost.set(applianceId, new Array(numRows).fill(0));
		} else {
			window.smceeEnergyData.disaggregatedEnergyData.insignificantAppliancesTotalEnergy += window.smceeEnergyData.disaggregatedEnergyData.applianceTotalEnergy[applianceId]
			window.smceeEnergyData.disaggregatedEnergyData.insignificantAppliancesTotalCost += window.smceeEnergyData.disaggregatedEnergyData.applianceTotalCost[applianceId]
		}
	}
	
	for(const element of receivedEnergyData) {
		let applianceEnergyTotal = 0;
		let index = energyDataType == "months" ? element.month - 1 : (energyDataType == "days" ? element.day - 1 : element.hour);
		
		for(let applianceId = 0; applianceId < applianceQty; applianceId++) {
			if(typeof element.appliance_energy[applianceId] == "undefined")
				break;
			
			if(element.appliance_energy[applianceId] == null)
				continue;
			
			if(window.smceeEnergyData.disaggregatedEnergyData.applianceEnergy.has(applianceId)) {
				window.smceeEnergyData.disaggregatedEnergyData.applianceEnergy.get(applianceId)[index] += element.appliance_energy[applianceId];
				window.smceeEnergyData.disaggregatedEnergyData.applianceCost.get(applianceId)[index] += element.appliance_cost[applianceId];
			} else {
				window.smceeEnergyData.disaggregatedEnergyData.insignificantAppliancesEnergy[index] += element.appliance_energy[applianceId];
				window.smceeEnergyData.disaggregatedEnergyData.insignificantAppliancesCost[index] += element.appliance_cost[applianceId];
			}
			
			applianceEnergyTotal += element.appliance_energy[applianceId];
		}
		
		window.smceeEnergyData.disaggregatedEnergyData.standbyEnergy[index] = element.standby_energy;
		window.smceeEnergyData.disaggregatedEnergyData.unknownEnergy[index] = Math.max(0, element.total_energy - applianceEnergyTotal);
	}
	
}

function fetchEnergyData(energyDataType, disaggregatedEnergy=false, comparisonData=false) {
	var date = disaggregatedEnergy ? window.smceeEnergyData.disaggregatedEnergyData.date : (comparisonData ? window.smceeEnergyData.energyComparisonData.date : window.smceeEnergyData.energyData.date);
	var xhrEnergyData = new XMLHttpRequest();
	var requestUrl = window.smceeApiUrlBase + (disaggregatedEnergy ? "disaggregated_energy/" : "energy/");
	
	document.getElementById('chart-type-select').disabled = true;
	document.getElementById('comparison-checkbox').disabled = true;
	document.getElementById('cost-checkbox').disabled = true;
	document.getElementById('coverage-checkbox').disabled = true;
	document.getElementById("disaggregated-energy-checkbox").disabled = true;
	
	xhrEnergyData.onload = function() {
		if(this.status == 200) {
			let responseObj = JSON.parse(this.responseText);
			let energyTarget = comparisonData ? window.smceeEnergyData.energyComparisonData : window.smceeEnergyData.energyData;
			
			if(disaggregatedEnergy) {
				formatDisaggregatedEnergyData(responseObj, energyDataType, window.smceeApplianceList.size);
			} else {
				formatEnergyData(responseObj, energyDataType, comparisonData)
			}
			
			updateChart();
			
			document.getElementById('chart-type-select').disabled = false;
			document.getElementById('comparison-checkbox').disabled = disaggregatedEnergy;
			document.getElementById('cost-checkbox').disabled = disaggregatedEnergy;
			document.getElementById('coverage-checkbox').disabled = disaggregatedEnergy;
			document.getElementById("disaggregated-energy-checkbox").disabled = (typeof window.smceeApplianceList == "undefined");
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch power data. Status: " + this.status);
		}
	}
	
	if(energyDataType == "months")
		requestUrl += "months?year=" + date;
	else if(energyDataType == "days")
		requestUrl += "days?year=" + date.split("-")[1] + "&month=" + date.split("-")[0];
	else
		requestUrl += "hours?year=" + date.split("-")[0] + "&month=" + date.split("-")[1] + "&day=" + date.split("-")[2];
	
	xhrEnergyData.open("GET", requestUrl);
	
	xhrEnergyData.timeout = 2000;
	
	xhrEnergyData.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrEnergyData.send();
}

function updateChart() {
	let selectedType = document.getElementById("chart-type-select").value;
	var comparisonCheckbox = document.getElementById("comparison-checkbox");
	var costCheckbox = document.getElementById('cost-checkbox');
	var coverageCheckbox = document.getElementById("coverage-checkbox");
	var disaggregatedEnergyCheckbox = document.getElementById("disaggregated-energy-checkbox");
	let selectedDate, selectedComparisonDate;
	
	if(disaggregatedEnergyCheckbox.checked) {
		comparisonCheckbox.checked = false;
		comparisonCheckbox.disabled = true;
		
		coverageCheckbox.checked = false;
		coverageCheckbox.disabled = true;
		
		costCheckbox.checked = false;
		costCheckbox.disabled = true;
		
		document.getElementById("energy-pie-chart-canvas").parentNode.classList.remove("is-hidden");
		
	} else {
		document.getElementById("energy-pie-chart-canvas").parentNode.classList.add("is-hidden");
		
		costCheckbox.disabled = false;
		
		if(comparisonCheckbox.checked) {
			document.getElementById("comparison-date-item").classList.remove("is-hidden");
			coverageCheckbox.checked = false;
			coverageCheckbox.disabled = true;
		} else {
			document.getElementById("comparison-date-item").classList.add("is-hidden");
			coverageCheckbox.disabled = false;
		}
	}
	
	if(selectedType == "months") {
		selectedDate = document.getElementById("date-year-select").value;
		selectedComparisonDate = document.getElementById("comparison-date-year-select").value;
	} else if(selectedType == "days") {
		selectedDate = document.getElementById("date-month-select").value;
		selectedComparisonDate = document.getElementById("comparison-date-month-select").value;
	} else {
		selectedDate = document.getElementById("date-day-input").value;
		selectedComparisonDate = document.getElementById("comparison-date-day-input").value;
	}
	
	if(selectedDate.length < 1 || (comparisonCheckbox.checked && selectedComparisonDate.length < 1))
		return;
	
	if(disaggregatedEnergyCheckbox.checked) {
		if(window.smceeEnergyData.disaggregatedEnergyData.date !== selectedDate) {
			window.smceeEnergyData.disaggregatedEnergyData.date = selectedDate;
			fetchEnergyData(selectedType, true, false);
			return;
		}
		
	} else {
		if(window.smceeEnergyData.energyData.date !== selectedDate) {
			window.smceeEnergyData.energyData.date = selectedDate;
			fetchEnergyData(selectedType, false, false);
			return;
		}
		
		if(comparisonCheckbox.checked && window.smceeEnergyData.energyComparisonData.date !== selectedComparisonDate) {
			window.smceeEnergyData.energyComparisonData.date = selectedComparisonDate;
			fetchEnergyData(selectedType, false, true);
			return;
		}
	}
	
	window.smceeEnergyBarChart.data.labels = [];
	
	if(selectedType == "hours") {
		for(let hour = 0; hour < 24; hour++)
			window.smceeEnergyBarChart.data.labels.push(hour + ":00");
		
	} else if(selectedType == "days") {
		let days = Math.max(daysInMonth(selectedDate.split("-")[0], selectedDate.split("-")[1]), (comparisonCheckbox.checked ? daysInMonth(selectedComparisonDate.split("-")[0], selectedComparisonDate.split("-")[1]) : 0));
		
		for(let day = 1; day <= days; day++)
			window.smceeEnergyBarChart.data.labels.push(day.toString());
		
	} else if(selectedType == "months") {
		for(let month of ["Jan", "Fev", "Mar", "Abr", "Mai", "Jun", "Jul", "Ago", "Set", "Out", "Nov", "Dez"])
			window.smceeEnergyBarChart.data.labels.push(month);
	}
	
	window.smceeEnergyBarChart.data.datasets = [];
	window.smceeEnergyPieChart.data.datasets = [];
	
	if(disaggregatedEnergyCheckbox.checked) {
		window.smceeEnergyPieChart.data.labels = [];
		window.smceeEnergyPieChart.data.datasets.push({
			label: 'Energia',
			backgroundColor: [],
			data: [],
		});
		
		window.smceeEnergyPieChart.data.datasets[0].data.push(window.smceeEnergyData.disaggregatedEnergyData.totalStandbyEnergy);
		window.smceeEnergyPieChart.data.datasets[0].backgroundColor.push('#3c3c3c');
		window.smceeEnergyPieChart.data.labels.push('Stand-by');
		
		window.smceeEnergyPieChart.data.datasets[0].data.push(window.smceeEnergyData.disaggregatedEnergyData.totalUnknownEnergy);
		window.smceeEnergyPieChart.data.datasets[0].backgroundColor.push('#b4b4b4');
		window.smceeEnergyPieChart.data.labels.push('Desconhecido');
		
		if(window.smceeEnergyData.disaggregatedEnergyData.insignificantAppliancesTotalEnergy > 0) {
			window.smceeEnergyPieChart.data.datasets[0].data.push(window.smceeEnergyData.disaggregatedEnergyData.insignificantAppliancesTotalEnergy);
			window.smceeEnergyPieChart.data.datasets[0].backgroundColor.push('#003388');
			window.smceeEnergyPieChart.data.labels.push('Outros');
		}
		
		window.smceeEnergyBarChart.data.datasets.push({
			label: 'Stand-by',
			backgroundColor: '#3c3c3c',
			data: window.smceeEnergyData.disaggregatedEnergyData.standbyEnergy,
			yAxisID: 'y',
		});
		
		window.smceeEnergyBarChart.data.datasets.push({
			label: 'Desconhecido',
			backgroundColor: '#b4b4b4',
			data: window.smceeEnergyData.disaggregatedEnergyData.unknownEnergy,
			yAxisID: 'y',
		});
		
		if(window.smceeEnergyData.disaggregatedEnergyData.insignificantAppliancesTotalEnergy > 0) {
			window.smceeEnergyBarChart.data.datasets.push({
				label: 'Outros',
				backgroundColor: '#003388',
				data: window.smceeEnergyData.disaggregatedEnergyData.insignificantAppliancesEnergy,
				yAxisID: 'y',
			});
		}
		
		for(const applianceId of window.smceeEnergyData.disaggregatedEnergyData.applianceEnergy.keys()) {
			window.smceeEnergyBarChart.data.datasets.push({
				label: window.smceeApplianceList.get(applianceId + 1).name,
				backgroundColor: window.smceeApplianceList.get(applianceId + 1).color,
				data: window.smceeEnergyData.disaggregatedEnergyData.applianceEnergy.get(applianceId),
				yAxisID: 'y',
			});
			
			window.smceeEnergyPieChart.data.datasets[0].data.push(window.smceeEnergyData.disaggregatedEnergyData.applianceTotalEnergy[applianceId]);
			window.smceeEnergyPieChart.data.datasets[0].backgroundColor.push(window.smceeApplianceList.get(applianceId + 1).color);
			window.smceeEnergyPieChart.data.labels.push(window.smceeApplianceList.get(applianceId + 1).name);
		}
		
		window.smceeEnergyPieChart.update();
		
	} else if(costCheckbox.checked) {
		window.smceeEnergyBarChart.data.datasets.push({
			label: 'Custo',
			backgroundColor: '#00d1b2',
			data: window.smceeEnergyData.energyData.cost,
			yAxisID: 'y',
		});
		
		if(comparisonCheckbox.checked) {
			window.smceeEnergyBarChart.data.datasets.push({
				label: 'Custo',
				backgroundColor: '#3e8ed0',
				data: window.smceeEnergyData.energyComparisonData.cost,
				yAxisID: 'y',
			});
		}
		
		window.smceeEnergyBarChart.options.scales.y.title.text = 'Custo (R$)';
	} else {
		window.smceeEnergyBarChart.data.datasets = [{
			label: 'Energia',
			backgroundColor: '#00d1b2',
			data: window.smceeEnergyData.energyData.energy,
			yAxisID: 'y',
		}];
		
		if(comparisonCheckbox.checked) {
			window.smceeEnergyBarChart.data.datasets.push({
				label: 'Energia',
				backgroundColor: '#3e8ed0',
				data: window.smceeEnergyData.energyComparisonData.energy,
				yAxisID: 'y',
			});
		}
		
		window.smceeEnergyBarChart.options.scales.y.title.text = 'Energia (kWh)';
	}
	
	if(!disaggregatedEnergyCheckbox.checked && coverageCheckbox.checked) {
		window.smceeEnergyBarChart.data.datasets.push({
			label: 'Cobertura',
			backgroundColor: '#b4b4b4',
			data: window.smceeEnergyData.energyData.coverage,
			type: 'line',
			yAxisID: 'y1',
		});
	}
	
	window.smceeEnergyBarChart.options.scales.x.stacked = disaggregatedEnergyCheckbox.checked;
	window.smceeEnergyBarChart.options.scales.y.stacked = disaggregatedEnergyCheckbox.checked;
	window.smceeEnergyBarChart.options.scales.y1.display = !disaggregatedEnergyCheckbox.checked && coverageCheckbox.checked;
	window.smceeEnergyBarChart.options.plugins.legend.display = disaggregatedEnergyCheckbox.checked;
	
	window.smceeEnergyBarChart.update();
}
