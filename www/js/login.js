window.onload = function () {
	
	authCheckAccessKey(redirectBack, null);
	
	document.getElementById("submit-button").onclick = submitLogin;
	
	document.getElementById("username-field").focus();
};

function redirectBack() {
	var redirectUrl = decodeURIComponent(new URLSearchParams(window.location.search).get("redirect"));
	var urlIsValid;
	
	try {
		urlIsValid = (new URL(redirectUrl).origin === window.location.origin);
	} catch(ev) {
		urlIsValid = false;
	}
	
	if(!urlIsValid) {
		redirectUrl = "/";
	}
	
	window.location.replace(redirectUrl);
}

function submitLogin() {
	var xhrLogin;
	var usernameField = document.getElementById("username-field");
	var passwordField = document.getElementById("password-field");
	var messageElement = document.getElementById("error-message");
	
	messageElement.innerHTML = "&nbsp;";
	usernameField.classList.remove("is-danger");
	passwordField.classList.remove("is-danger");
	
	if(usernameField.value.length < 1) {
		usernameField.classList.add("is-danger");
		return;
	}
	
	if(passwordField.value.length < 1) {
		passwordField.classList.add("is-danger");
		return;
	}
	
	xhrLogin = new XMLHttpRequest()
	
	xhrLogin.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			if(responseObject.result === "success" && typeof responseObject.access_key === "string") {
				localStorage.setItem("access_key", responseObject.access_key);
				redirectBack();
				
			} else if(responseObject.result === "wrong") {
				messageElement.innerText = "Dados incorretos";
				
				usernameField.classList.add("is-danger");
				passwordField.classList.add("is-danger");
				
			} else if(responseObject.result === "inactive") {
				messageElement.innerText = "Usuário inativo";
				
				usernameField.classList.add("is-danger");
			}
			
		} else {
			console.error("Failed to login. Status: " + this.status);
			return;
		}
	};
	
	xhrLogin.open("POST", window.smceeApiUrlBase + "auth/login", true);
	
	xhrLogin.timeout = 2000;
	
	xhrLogin.send(JSON.stringify({"username":usernameField.value,"password":passwordField.value}));
}
