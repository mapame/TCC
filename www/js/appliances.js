function hsvToRGB(hue, saturation, value) {
	var red;
	var green;
	var blue;
	if(saturation === 0) {
		red = value;
		green = value;
		blue = value;
	} else {
		var i = Math.floor(hue * 6);
		var f = hue * 6 - i;
		var p = value * (1 - saturation);
		var q = value * (1 - saturation * f);
		var t = value * (1 - saturation * (1 - f));
		switch (i) {
			case 1:
				red = q;
				green = value;
				blue = p;
				break;
			case 2:
				red = p;
				green = value;
				blue = t;
				break;
			case 3:
				red = p;
				green = q;
				blue = value;
				break;
			case 4:
				red = t;
				green = p;
				blue = value;
				break;
			case 5:
				red = value;
				green = p;
				blue = q;
				break;
			case 6: // fall through
			case 0:
				red = value;
				green = t;
				blue = p;
				break;
		}
	}
	red = Math.floor(255 * red + 0.5);
	green = Math.floor(255 * green + 0.5);
	blue = Math.floor(255 * blue + 0.5);
	
	return "#" + red.toString(16).padStart(2, "0") + green.toString(16).padStart(2, "0") + blue.toString(16).padStart(2, "0");
}

function fetchApplianceList(completionCallback) {
	var xhrApplianceList = new XMLHttpRequest();
	let colorHue = 0.2;
	let colorSat = 0.8;
	let colorVal = 0.7;
	const goldenRatio = 1.61803398875;
	
	xhrApplianceList.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			window.smceeApplianceList = new Map();
			
			for(let applianceItem of responseObject) {
				applianceItem.color = hsvToRGB(colorHue, colorSat, colorVal);
				
				window.smceeApplianceList.set(applianceItem.id, applianceItem);
				
				colorHue = (colorHue + 1/goldenRatio) % 1;
			}
			
			if(typeof completionCallback == "function")
				completionCallback();
			
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
