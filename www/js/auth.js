function authCheckAccessKey(validCallback, invalidCallback, errorCallback) {
	var xhrCheckKey;
	var access_key;
	
	if((access_key = localStorage.getItem("access_key")) === null) {
		if(typeof invalidCallback === "function") {
			invalidCallback();
		}
		
		return;
	}
	
	xhrCheckKey = new XMLHttpRequest()
	
	xhrCheckKey.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			if(responseObject.result === "valid") {
				if(typeof validCallback === "function") {
					validCallback();
				}
			} else if(responseObject.result === "invalid") {
				localStorage.removeItem("access_key");
				
				if(typeof invalidCallback === "function") {
					invalidCallback();
				}
			}
			
		} else {
			console.error("Failed to check access key. Status: " + this.status);
			
			if(typeof errorCallback === "function") {
					errorCallback();
			}
		}
	};
	
	xhrCheckKey.open("GET", window.smceeApiUrlBase + "auth/verify", true);
	
	xhrCheckKey.timeout = 2000;
	
	xhrCheckKey.setRequestHeader("Authorization", "Bearer " + access_key);
	
	xhrCheckKey.send();
}

function redirectToLogin() {
	var loginURL = new URL(window.location.href);
	
	loginURL.pathname = "/login.html";
	loginURL.search = "";
	loginURL.searchParams.set("redirect", encodeURIComponent(window.location));
	
	window.location.replace(loginURL.href);
}

function authLogout() {
	var xhrLogout;
	var access_key;
	
	if((access_key = localStorage.getItem("access_key")) === null) {
		redirectToLogin()
		
		return;
	}
	
	xhrLogout = new XMLHttpRequest()
	
	xhrLogout.onload = function() {
		if(this.status === 200) {
			localStorage.removeItem("access_key");
			redirectToLogin();
		} else {
			console.error("Failed to logout. Status: " + this.status);
		}
	};
	
	xhrLogout.open("POST", window.smceeApiUrlBase + "auth/logout", true);
	
	xhrLogout.timeout = 2000;
	
	xhrLogout.setRequestHeader("Authorization", "Bearer " + access_key);
	
	xhrLogout.send();
}
