var lastTimestamp = 0;
var displaytime = new Date();
var tpPoints = [];
var tsPoints = [];
var tqPoints = [];

window.onload = function () {
function reducesum(total, value, index, array) {
  return total + value;
} 

function updateChart() {
	var request = new XMLHttpRequest();
	var req_url = "http://" + window.location.hostname + ":8081/?startTime=" + lastTimestamp;
	
	request.onload = function() {
		if(this.status == 200) {
			var responseObj = JSON.parse(this.responseText);
			
			if(responseObj.power_data.length == 0) {
				setTimeout(updateChart, 500);
				return;
			}
			
			lastTimestamp = responseObj.power_data[0].timestamp;
			
			displaytime.setTime(lastTimestamp * 1000);
			
			document.getElementById("data_time").innerHTML = displaytime.getHours().toString().padStart(2, '0') + ":" + displaytime.getMinutes().toString().padStart(2, '0') + ":" + displaytime.getSeconds().toString().padStart(2, '0');
			
			document.getElementById("va").innerHTML = responseObj.power_data[0].v[0].toFixed(2) + " V";
			document.getElementById("vb").innerHTML = responseObj.power_data[0].v[1].toFixed(2) + " V";
			
			document.getElementById("ia").innerHTML = responseObj.power_data[0].i[0].toFixed(3) + " A";
			document.getElementById("ib").innerHTML = responseObj.power_data[0].i[1].toFixed(3) + " A";
			
			document.getElementById("pa").innerHTML = responseObj.power_data[0].p[0].toFixed(1) + " W";
			document.getElementById("pb").innerHTML = responseObj.power_data[0].p[1].toFixed(1) + " W";
			document.getElementById("papb").innerHTML = (responseObj.power_data[0].p[0] + responseObj.power_data[0].p[1]).toFixed(1) + " W";
			
			document.getElementById("sa").innerHTML = responseObj.power_data[0].s[0].toFixed(1) + " VA";
			document.getElementById("sb").innerHTML = responseObj.power_data[0].s[1].toFixed(1) + " VA";
			document.getElementById("sapb").innerHTML = (responseObj.power_data[0].s[0] + responseObj.power_data[0].s[1]).toFixed(1) + " VA";
			
			document.getElementById("qa").innerHTML = responseObj.power_data[0].q[0].toFixed(1) + " VAr";
			document.getElementById("qb").innerHTML = responseObj.power_data[0].q[1].toFixed(1) + " VAr";
			document.getElementById("qapb").innerHTML = (responseObj.power_data[0].q[0] + responseObj.power_data[0].q[1]).toFixed(1);
			
			document.getElementById("pfa").innerHTML = responseObj.power_data[0].pf[0].toFixed(2);
			document.getElementById("pfb").innerHTML = responseObj.power_data[0].pf[1].toFixed(2);
			
			responseObj.power_data.reverse();
			
			for (i in responseObj.power_data) {
				chart1.data.datasets[0].data.push({
					t: new Date(responseObj.power_data[i].timestamp * 1000),
					y: responseObj.power_data[i].p[0] + responseObj.power_data[i].p[1]
				});
				chart1.data.datasets[1].data.push({
					t: new Date(responseObj.power_data[i].timestamp * 1000),
					y: responseObj.power_data[i].s[0] + responseObj.power_data[i].s[1]
				});
				chart1.data.datasets[2].data.push({
					t: new Date(responseObj.power_data[i].timestamp * 1000),
					y: responseObj.power_data[i].q[0] + responseObj.power_data[i].q[1]
				});
			}
			
			chart1.update();
			
			setTimeout(updateChart, 1000);
		} else {
			setTimeout(updateChart, 500);
		}
	}
	
	request.open("GET", req_url, true);
	request.send();
}

var chart1ctx = document.getElementById('chart1');

var chart1 = new Chart(chart1ctx, {
	type: 'line',
	data: {
		datasets: [{
			label: 'Total Active Power',
			borderColor: 'rgb(100, 100, 255)',
			fill: false,
			data: []
		},
		{
			label: 'Total Apparent Power',
			borderColor: 'rgb(100, 255, 100)',
			fill: false,
			data: []
		},
		{
			label: 'Total Reactive Power',
			borderColor: 'rgb(255, 100, 100)',
			fill: false,
			data: []
		}]
	},
	options: {
		scales: {
			xAxes: [{
				type: 'time',
			}],
			yAxes: [{
			}]
		},
		animation: {
			duration: 0
		},
		tooltips: {
			mode: 'index',
			intersect: false
		}
	}
});

updateChart();
}
