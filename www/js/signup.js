window.onload = function () {
	
	authCheckAccessKey(function() { window.location.replace("/"); }, null);
	
	document.getElementById("submit-button").onclick = submitSignup;
};

function submitSignup() {
	var xhrSubmit;
	var usernameField = document.getElementById("username-field");
	var passwordField = document.getElementById("password-field");
	var passwordConfirmationField = document.getElementById("password-confirmation-field");
	var messageElement = document.getElementById("error-message");
	
	messageElement.innerHTML = "&nbsp;";
	usernameField.classList.remove("is-danger");
	passwordField.classList.remove("is-danger");
	passwordConfirmationField.classList.remove("is-danger");
	
	if(usernameField.value.length < 3) {
		messageElement.innerHTML = "Usuário muito curto";
		usernameField.classList.add("is-danger");
		return;
	}
	
	if(passwordField.value.length < 4) {
		messageElement.innerHTML = "Senha muito curta";
		passwordField.classList.add("is-danger");
		passwordConfirmationField.classList.add("is-danger");
		return;
	}
	
	if(passwordField.value !== passwordConfirmationField.value) {
		messageElement.innerHTML = "As senhas não são iguais";
		passwordField.classList.add("is-danger");
		passwordConfirmationField.classList.add("is-danger");
		return;
	}
	
	xhrSubmit = new XMLHttpRequest()
	
	xhrSubmit.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			if(responseObject.result === "success") {
				alert("Usuário cadastrado, aguarde a ativação da conta por um administrador para poder entrar.");
				window.location.replace("/login.html");
				
			} else if(responseObject.result === "user_name_taken") {
				messageElement.innerText = "Nome de usuário já existe";
				
				usernameField.classList.add("is-danger");
			}
			
		} else {
			console.error("Failed to login. Status: " + this.status);
			return;
		}
	};
	
	xhrSubmit.open("POST", window.smceeApiUrlBase + "users", true);
	
	xhrSubmit.timeout = 2000;
	
	xhrSubmit.send(JSON.stringify({"name":usernameField.value,"password":passwordField.value}));
}
