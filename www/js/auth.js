function authCheckAccessKey(successCallback, failureCallback) {
	var xhr_check_key;
	var access_key;
	
	if((access_key = localStorage.getItem("access_key")) === null) {
		return;
	}
	
	xhr_check_key = new XMLHttpRequest()
	
	xhr_check_key.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			if(responseObject.result === "valid") {
				if(typeof successCallback === "function") {
					successCallback();
				}
			} else if(responseObject.result === "invalid") {
				localStorage.removeItem("access_key");
				
				if(typeof failureCallback === "function") {
					failureCallback();
				}
			}
			
		} else {
			console.error("Failed to check access key. Status: " + this.status);
			return;
		}
	};
	
	xhr_check_key.open("GET", window.smceeApiUrlBase + "auth/verify", true);
	
	xhr_check_key.timeout = 2000;
	
	xhr_check_key.setRequestHeader("Authorization", "Bearer " + access_key);
	
	xhr_check_key.send();
}
