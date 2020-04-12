var lastTimestamp = 0;
var displaytime = new Date();
var powerdata_total = [];
var powerdata_a = [];
var powerdata_b = [];
var voltagedata = [];

window.onload = function () {
function updateChart() {
	var request = new XMLHttpRequest();
	var req_url = "http://" + window.location.hostname + ":8081/?startTime=" + lastTimestamp;
	
	request.onreadystatechange = function() {
		if(this.readyState == 4) {
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
				document.getElementById("pt").innerHTML = (responseObj.power_data[0].p[0] + responseObj.power_data[0].p[1]).toFixed(1) + " W";
				
				document.getElementById("sa").innerHTML = responseObj.power_data[0].s[0].toFixed(1) + " VA";
				document.getElementById("sb").innerHTML = responseObj.power_data[0].s[1].toFixed(1) + " VA";
				document.getElementById("st").innerHTML = (responseObj.power_data[0].s[0] + responseObj.power_data[0].s[1]).toFixed(1) + " VA";
				
				document.getElementById("qa").innerHTML = responseObj.power_data[0].q[0].toFixed(1) + " VAr";
				document.getElementById("qb").innerHTML = responseObj.power_data[0].q[1].toFixed(1) + " VAr";
				document.getElementById("qt").innerHTML = (responseObj.power_data[0].q[0] + responseObj.power_data[0].q[1]).toFixed(1) + " VAr";
				
				document.getElementById("pfa").innerHTML = responseObj.power_data[0].pf[0].toFixed(2);
				document.getElementById("pfb").innerHTML = responseObj.power_data[0].pf[1].toFixed(2);
				document.getElementById("pft").innerHTML = ((responseObj.power_data[0].p[0] + responseObj.power_data[0].p[1]) / (responseObj.power_data[0].s[0] + responseObj.power_data[0].s[1])).toFixed(2);
				
				responseObj.power_data.reverse();
				
				for (i in responseObj.power_data) {
					powerdata_total.push([
						new Date(responseObj.power_data[i].timestamp * 1000),
						responseObj.power_data[i].p[0] + responseObj.power_data[i].p[1],
						responseObj.power_data[i].s[0] + responseObj.power_data[i].s[1],
						responseObj.power_data[i].q[0] + responseObj.power_data[i].q[1]
					]);
					
					powerdata_a.push([
						new Date(responseObj.power_data[i].timestamp * 1000),
						responseObj.power_data[i].p[0],
						responseObj.power_data[i].s[0],
						responseObj.power_data[i].q[0]
					]);
					
					powerdata_b.push([
						new Date(responseObj.power_data[i].timestamp * 1000),
						responseObj.power_data[i].p[1],
						responseObj.power_data[i].s[1],
						responseObj.power_data[i].q[1]
					]);
					
					voltagedata.push([
						new Date(responseObj.power_data[i].timestamp * 1000),
						responseObj.power_data[i].v[0],
						responseObj.power_data[i].v[1]
					]);
				}
				
				chart_total_power.updateOptions({'file' : powerdata_total});
				chart_power_a.updateOptions({'file' : powerdata_a});
				chart_power_b.updateOptions({'file' : powerdata_b});
				chart_voltage.updateOptions({'file' : voltagedata});
				
				setTimeout(updateChart, 1000);
			} else {
				setTimeout(updateChart, 500);
			}
		}
	}
	
	request.open("GET", req_url, true);
	request.send();
}

var chart_total_power = new Dygraph(document.getElementById("total_power_chart_div"), [[0, 0, 0, 0]],
	{
		labels: ["Time", "Active power", "Apparent Power", "Reative Power"],
		drawPoints: false,
	});
	
var chart_power_a = new Dygraph(document.getElementById("chart2Adiv"), [[0, 0, 0, 0]],
	{
		labels: ["Time", "Active power", "Apparent Power", "Reative Power"],
		drawPoints: false,
	});
	
var chart_power_b = new Dygraph(document.getElementById("chart2Bdiv"), [[0, 0, 0, 0]],
	{
		labels: ["Time", "Active power", "Apparent Power", "Reative Power"],
		drawPoints: false,
	});
	
var chart_voltage = new Dygraph(document.getElementById("voltage_chart_div"), [[0, 0, 0]],
	{
		labels: ["Time", "Phase A", "Phase B"],
		valueRange: [110, 140],
		drawPoints: false,
	});

updateChart();
}
