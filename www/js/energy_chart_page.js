window.onload = function() {
	authCheckAccessKey(initPage, redirectToLogin);
	addEventsNavbarBurgers();
}

function initPage() {
	userInfoFetch(function() {navbarPopulateItems("main-menu");});
	
	fetchEnergyDateInfo();
	
	window.smceeEnergyData = {};
	window.smceeEnergyData.type = "";
	window.smceeEnergyData.energyData = {};
	window.smceeEnergyData.energyComparisonData = {};
	
	document.getElementById("chart-type-select").onchange = changeChartType;
	document.getElementById("comparison-checkbox").onchange = updateChart;
	document.getElementById("cost-checkbox").onchange = updateChart;
	document.getElementById("coverage-checkbox").onchange = updateChart;
	
	document.getElementById("date-day-input").onchange = updateChart;
	document.getElementById("date-month-select").onchange = updateChart;
	document.getElementById("date-year-select").onchange = updateChart;
	
	document.getElementById("comparison-date-day-input").onchange = updateChart;
	document.getElementById("comparison-date-month-select").onchange = updateChart;
	document.getElementById("comparison-date-year-select").onchange = updateChart;
	
	window.smceeEnergyChart = new Chart(document.getElementById('energy-chart-canvas'), {
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
					display: false
				}
			}
		}
	});
}

function daysInMonth(month, year) {
    return new Date(year, month, 0).getDate();
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
	
	/*
	if(selectedType == "minutes") {
		document.getElementById('comparison-checkbox').checked = false;
		document.getElementById("comparison-date-item").classList.add("is-hidden");
	}
	*/
	
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

function fetchEnergyData(energyDataType, comparisonData=false) {
	var date = comparisonData ? window.smceeEnergyData.energyComparisonData.date : window.smceeEnergyData.energyData.date;
	var xhrEnergyData = new XMLHttpRequest();
	var requestUrl = window.smceeApiUrlBase + "energy/";
	
	document.getElementById('chart-type-select').disabled = true;
	document.getElementById('comparison-checkbox').disabled = true;
	document.getElementById('cost-checkbox').disabled = true;
	document.getElementById('coverage-checkbox').disabled = true;
	
	xhrEnergyData.onload = function() {
		if(this.status == 200) {
			let responseObj = JSON.parse(this.responseText);
			let energyTarget = comparisonData ? window.smceeEnergyData.energyComparisonData : window.smceeEnergyData.energyData;
			
			energyTarget.energy = [];
			energyTarget.cost = [];
			energyTarget.coverage = [];
			
			for(const element of responseObj) {
				let idx = (energyDataType == "months" ? element.month - 1 : (energyDataType == "days" ? element.day - 1 : element.hour));
				let totalSeconds = 3600 * (energyDataType == "months" ? daysInMonth(element.month, Number(date)) * 24 : (energyDataType == "days" ? 24 : 1));
				
				energyTarget.energy[idx] = element.active.toFixed(2);
				energyTarget.cost[idx] = element.cost.toFixed(2);
				energyTarget.coverage[idx] = ((element.second_count / totalSeconds) * 100).toFixed(1);
			}
			
			document.getElementById('chart-type-select').disabled = false;
			document.getElementById('comparison-checkbox').disabled = false;
			document.getElementById('cost-checkbox').disabled = false;
			document.getElementById('coverage-checkbox').disabled = false;
			
			updateChart();
			
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
	let selectedDate, selectedComparisonDate;
	
	if(comparisonCheckbox.checked) {
		document.getElementById("comparison-date-item").classList.remove("is-hidden");
		coverageCheckbox.checked = false;
		coverageCheckbox.disabled = true;
	} else {
		document.getElementById("comparison-date-item").classList.add("is-hidden");
		coverageCheckbox.disabled = false;
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
	
	if(window.smceeEnergyData.energyData.date !== selectedDate) {
		window.smceeEnergyData.energyData.date = selectedDate;
		fetchEnergyData(selectedType, false);
		return;
	}
	
	if(comparisonCheckbox.checked && window.smceeEnergyData.energyComparisonData.date !== selectedComparisonDate) {
		window.smceeEnergyData.energyComparisonData.date = selectedComparisonDate;
		fetchEnergyData(selectedType, true);
		return;
	}
	
	window.smceeEnergyChart.data.labels = [];
	
	if(selectedType == "hours") {
		for(let hour = 0; hour < 24; hour++)
			window.smceeEnergyChart.data.labels.push(hour + ":00");
		
	} else if(selectedType == "days") {
		let days = Math.max(daysInMonth(selectedDate.split("-")[0], selectedDate.split("-")[1]), (comparisonCheckbox.checked ? daysInMonth(selectedComparisonDate.split("-")[0], selectedComparisonDate.split("-")[1]) : 0));
		
		for(let day = 1; day <= days; day++)
			window.smceeEnergyChart.data.labels.push(day.toString());
		
	} else if(selectedType == "months") {
		for(let month of ["Jan", "Fev", "Mar", "Abr", "Mai", "Jun", "Jul", "Ago", "Set", "Out", "Nov", "Dez"])
			window.smceeEnergyChart.data.labels.push(month);
	}
	
	window.smceeEnergyChart.data.datasets = [];
	
	if(costCheckbox.checked) {
		window.smceeEnergyChart.data.datasets.push({
			label: 'Custo',
			backgroundColor: 'rgb(0, 209, 178)',
			data: window.smceeEnergyData.energyData.cost,
			yAxisID: 'y',
		});
		
		if(comparisonCheckbox.checked) {
			window.smceeEnergyChart.data.datasets.push({
				label: 'Custo',
				backgroundColor: 'rgb(62, 142, 208)',
				data: window.smceeEnergyData.energyComparisonData.cost,
				yAxisID: 'y',
			});
		}
		
		window.smceeEnergyChart.options.scales.y.title.text = 'Custo (R$)';
	} else {
		window.smceeEnergyChart.data.datasets = [{
			label: 'Energia',
			backgroundColor: 'rgb(0, 209, 178)',
			data: window.smceeEnergyData.energyData.energy,
			yAxisID: 'y',
		}];
		
		if(comparisonCheckbox.checked) {
			window.smceeEnergyChart.data.datasets.push({
				label: 'Energia',
				backgroundColor: 'rgb(62, 142, 208)',
				data: window.smceeEnergyData.energyComparisonData.energy,
				yAxisID: 'y',
			});
		}
		
		window.smceeEnergyChart.options.scales.y.title.text = 'Energia (kWh)';
	}
	
	if(coverageCheckbox.checked) {
		window.smceeEnergyChart.data.datasets.push({
			label: 'Cobertura',
			backgroundColor: 'rgb(180, 180, 180)',
			data: window.smceeEnergyData.energyData.coverage,
			type: 'line',
			yAxisID: 'y1',
		});
	}
	
	window.smceeEnergyChart.options.scales.y1.display = coverageCheckbox.checked;
	
	window.smceeEnergyChart.update();
}
