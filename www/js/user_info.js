function userInfoFetch(doneCallback, errorCallback) {
	var accessKey = localStorage.getItem("access_key");
	var xhrUserInfo;
	
	window.loggedUserInfo = null;
	
	if(accessKey === null) {
		return;
	}
	
	xhrUserInfo = new XMLHttpRequest()
	
	xhrUserInfo.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			if(typeof responseObject === "object") {
				window.loggedUserInfo = responseObject;
				
				if(typeof doneCallback === "function") {
					doneCallback();
				}
			}
		} else {
			console.error("Failed to fetch user info. Status: " + this.status);
			
			if(typeof errorCallback === "function") {
				errorCallback();
			}
		}
	};
	
	xhrUserInfo.open("GET", window.smceeApiUrlBase + "users/self", true);
	
	xhrUserInfo.timeout = 2000;
	
	xhrUserInfo.setRequestHeader("Authorization", "Bearer " + accessKey);
	
	xhrUserInfo.send();
}
