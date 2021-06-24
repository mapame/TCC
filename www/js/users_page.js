window.onload = function() {
	authCheckAccessKey(initPage, redirectToLogin);
	addEventsNavbarBurgers();
}

function initPage() {
	userInfoFetch(function() {navbarPopulateItems("main-menu");});
	
	document.getElementById("button-open-add-user").onclick = openAddUserModal;
	document.getElementById("inactive-filter-checkbox").onchange = updateUserTable;
	
	document.getElementById("button-discard-user").onclick = modalCloseAll;
	
	fetchUserList();
}

function openAddUserModal() {
	document.getElementById("modal-title-user").innerText = "Adicionar novo usuário";
	
	document.getElementById("input-user-name").value = "";
	document.getElementById("input-user-is-active").checked = false;
	document.getElementById("input-user-is-admin").checked = false;
	
	document.getElementById("input-user-password").value = "";
	document.getElementById("input-user-password").placeholder = "";
	document.getElementById("input-user-password-confirmation").value = "";
	document.getElementById("input-user-password-confirmation").placeholder = "";
	
	document.getElementById("button-submit-user").onclick = submitNewUser;
	
	modalOpen("modal-user");
}

function openEditUserModal(userId) {
	var user = window.smceeUserList.get(userId);
	
	document.getElementById("modal-title-user").innerText = "Editar usuário " + userId;
	
	document.getElementById("input-user-name").value = user.name;
	document.getElementById("input-user-is-active").checked = user.is_active;
	document.getElementById("input-user-is-admin").checked = user.is_admin;
	
	document.getElementById("input-user-password").value = "";
	document.getElementById("input-user-password").placeholder = "Deixe em branco para manter a senha atual.";
	document.getElementById("input-user-password-confirmation").value = "";
	document.getElementById("input-user-password-confirmation").placeholder = "Deixe em branco para manter a senha atual.";
	
	document.getElementById("button-submit-user").onclick = submitUpdatedUser.bind(null, userId);
	
	modalOpen("modal-user");
}

function userTableAddItem(item) {
	var tableNewRow = document.createElement("tr");
	var tableCellID = document.createElement("th");
	var tableCellName = document.createElement("td");
	var tableCellDateCreation = document.createElement("td");
	var tableCellDateModification = document.createElement("td");
	var tableCellActions = document.createElement("td");
	var actionButton = document.createElement("button");
	var actionButtonIconContainer = document.createElement("span");
	var actionButtonIcon = document.createElement("i");
	
	document.getElementById("users-tbody").appendChild(tableNewRow);
	
	tableNewRow.appendChild(tableCellID);
	tableNewRow.appendChild(tableCellName);
	tableNewRow.appendChild(tableCellDateCreation);
	tableNewRow.appendChild(tableCellDateModification);
	tableNewRow.appendChild(tableCellActions);
	
	tableCellActions.appendChild(actionButton);
	actionButton.appendChild(actionButtonIconContainer);
	actionButtonIconContainer.appendChild(actionButtonIcon);
	
	tableCellName.className = "has-text-centered is-vcentered";
	tableCellDateCreation.className = "has-text-centered is-vcentered is-hidden-mobile";
	tableCellDateModification.className = "has-text-centered is-vcentered is-hidden-mobile";
	tableCellActions.className = "has-text-right is-vcentered";
	
	actionButton.className = "button is-small is-info";
	actionButtonIconContainer.className = "icon is-small";
	actionButtonIcon.className = "mdi mdi-18px mdi-file-edit";
	
	actionButton.onclick = openEditUserModal.bind(null, item.id);
	
	tableNewRow.className = item.is_active ? "" : "has-text-danger";
	
	tableCellID.innerText = item.id;
	tableCellName.innerText = item.name;
	tableCellDateCreation.innerText = new Date(item.creation_date * 1000).toLocaleString("pt-BR");
	tableCellDateModification.innerText = new Date(item.modification_date * 1000).toLocaleString("pt-BR");
	
	if(item.is_admin) {
		let adminIcon = document.createElement("i");
		
		adminIcon.className = "ml-1 mdi mdi-star";
		
		tableCellName.appendChild(adminIcon);
	}
}

function updateUserTable() {
	var inactiveFilterCheckbox = document.getElementById("inactive-filter-checkbox");
	var userTableBody = document.getElementById("users-tbody");
	
	if(typeof window.smceeUserList != "object")
		return;
	
	while(userTableBody.lastChild)
		userTableBody.removeChild(userTableBody.lastChild);
	
	for(user of window.smceeUserList.values()) {
		if(inactiveFilterCheckbox !== null && inactiveFilterCheckbox.checked && !user.is_active)
			continue;
		
		userTableAddItem(user);
	}
}

function fetchUserList() {
	var xhrUserList = new XMLHttpRequest();
	
	xhrUserList.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			window.smceeUserList = new Map();
			
			for(user of responseObject)
				window.smceeUserList.set(user.id, user);
			
			updateUserTable();
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch user list. Status: " + this.status);
		}
	}
	
	xhrUserList.open("GET", window.smceeApiUrlBase + "users");
	
	xhrUserList.timeout = 2000;
	
	xhrUserList.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrUserList.send();
}

function validateUserForm(allowEmptyPassword=false) {
	var fieldName = document.getElementById("input-user-name");
	var fieldPassword = document.getElementById("input-user-password");
	var fieldPasswordConfirmation = document.getElementById("input-user-password-confirmation");
	
	fieldName.classList.remove("is-danger");
	fieldPassword.classList.remove("is-danger");
	fieldPasswordConfirmation.classList.remove("is-danger");
	
	if(fieldName.value.length < 3 || fieldName.value.length > 16) {
		fieldName.classList.add("is-danger");
		return false;
	}
	
	if(allowEmptyPassword && fieldPassword.value.length == 0 && fieldPasswordConfirmation.value.length == 0)
		return true;
	
	if(fieldPassword.value.length < 4 || fieldPassword.value.length > 16) {
		fieldPassword.classList.add("is-danger");
		fieldPasswordConfirmation.classList.add("is-danger");
		return false;
	}
	
	if(fieldPassword.value !== fieldPasswordConfirmation.value) {
		fieldPasswordConfirmation.classList.add("is-danger");
		return false;
	}
	
	return true;
}

function submitNewUser() {
	var xhrSubmitNewUser;
	var newUser;
	
	if(!validateUserForm())
		return;
	
	newUser = {
			"name": document.getElementById("input-user-name").value,
			"is_active": document.getElementById("input-user-is-active").checked,
			"is_admin": document.getElementById("input-user-is-admin").checked,
			"password": document.getElementById("input-user-password").value
	};
	
	document.getElementById("button-submit-user").classList.add("is-loading");
	
	xhrSubmitNewUser = new XMLHttpRequest()
	
	xhrSubmitNewUser.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			if(responseObject.result === "success" && typeof responseObject.user_id == "number") {
				newUser.id = responseObject.user_id;
				newUser.modification_date = newUser.creation_date = Math.round(new Date() / 1000);
				
				window.smceeUserList.set(responseObject.user_id, newUser);
				updateUserTable();
				
				modalCloseAll();
				
			} else if(responseObject.result === "failed") {
				alert("Erro ao salvar novo aparelho.");
			}
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to submit new user. Status: " + this.status);
		}
		
		document.getElementById("button-submit-user").classList.remove("is-loading");
	};
	
	xhrSubmitNewUser.open("POST", window.smceeApiUrlBase + "users", true);
	
	xhrSubmitNewUser.timeout = 2000;
	
	xhrSubmitNewUser.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrSubmitNewUser.send(JSON.stringify(newUser));
}

function submitUpdatedUser(userId) {
	var xhrUpdateUser;
	var updatedUser;
	
	if(!validateUserForm(true))
		return;
	
	updatedUser = {
			"name": document.getElementById("input-user-name").value,
			"is_active": document.getElementById("input-user-is-active").checked,
			"is_admin": document.getElementById("input-user-is-admin").checked
	};
	
	if(document.getElementById("input-user-password").value.length > 0)
		updatedUser.password = document.getElementById("input-user-password").value;
	
	document.getElementById("button-submit-user").classList.add("is-loading");
	
	xhrUpdateUser = new XMLHttpRequest()
	
	xhrUpdateUser.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			if(responseObject.result === "success") {
				updatedUser.id = userId;
				updatedUser.creation_date = window.smceeUserList.get(userId).creation_date;
				updatedUser.modification_date = Math.round(new Date() / 1000);
				updatedUser.password = "";
				
				window.smceeUserList.set(userId, updatedUser);
				
				updateUserTable();
				
				modalCloseAll();
			}
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to submit updated user. Status: " + this.status);
		}
		
		document.getElementById("button-submit-user").classList.remove("is-loading");
	};
	
	xhrUpdateUser.open("PUT", window.smceeApiUrlBase + "users/" + userId, true);
	
	xhrUpdateUser.timeout = 2000;
	
	xhrUpdateUser.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrUpdateUser.send(JSON.stringify(updatedUser));
	
}
