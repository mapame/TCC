window.onload = function() {
	authCheckAccessKey(initPage, redirectToLogin);
	addEventsNavbarBurgers();
}

function initPage() {
	userInfoFetch(function() {navbarPopulateItems("main-menu");});
	
	document.getElementById("button-open-add-appliance").onclick = openAddApplianceModal;
	document.getElementById("inactive-filter-checkbox").onchange = updateApplianceTable;
	document.getElementById("button-discard-appliance").onclick = modalCloseAll;
	
	fetchApplianceList(updateApplianceTable);
}

function openAddApplianceModal() {
	document.getElementById("modal-title-appliance").innerText = "Adicionar novo aparelho";
	
	document.getElementById("input-appliance-name").value = "";
	document.getElementById("input-appliance-is-active").checked = false;
	document.getElementById("input-appliance-max-time-on").value = "0";
	document.getElementById("input-appliance-is-hardwired").checked = false;
	
	document.getElementById("button-submit-appliance").onclick = submitNewAppliance;
	
	modalOpen("modal-appliance");
}

function openEditApplianceModal(applianceId) {
	var applianceItem = window.smceeApplianceList.get(applianceId);
	
	document.getElementById("modal-title-appliance").innerText = "Editar aparelho " + applianceId;
	
	document.getElementById("input-appliance-name").value = applianceItem.name;
	document.getElementById("input-appliance-is-active").checked = applianceItem.is_active;
	document.getElementById("input-appliance-max-time-on").value = applianceItem.max_time_on;
	document.getElementById("input-appliance-is-hardwired").checked = applianceItem.is_hardwired;
	
	document.getElementById("button-submit-appliance").onclick = submitUpdateAppliance.bind(null, applianceId);
	
	modalOpen("modal-appliance");
}

function applianceTableAddItem(item) {
	var tableNewRow = document.createElement("tr");
	var tableCellID = document.createElement("th");
	var tableCellName = document.createElement("td");
	var tableCellPower = document.createElement("td");
	var tableCellIsHardwired = document.createElement("td");
	var tableCellSigQty = document.createElement("td");
	var tableCellDateCreation = document.createElement("td");
	var tableCellDateModification = document.createElement("td");
	var tableCellActions = document.createElement("td");
	var actionButtonModify = document.createElement("button");
	var actionButtonIconContModify = document.createElement("span");
	var actionButtonIconModify = document.createElement("i");
	
	document.getElementById("appliance-tbody").appendChild(tableNewRow);
	
	tableNewRow.appendChild(tableCellID);
	tableNewRow.appendChild(tableCellName);
	tableNewRow.appendChild(tableCellPower);
	tableNewRow.appendChild(tableCellIsHardwired);
	tableNewRow.appendChild(tableCellSigQty);
	tableNewRow.appendChild(tableCellDateCreation);
	tableNewRow.appendChild(tableCellDateModification);
	tableNewRow.appendChild(tableCellActions);
	
	tableCellActions.appendChild(actionButtonModify);
	actionButtonModify.appendChild(actionButtonIconContModify);
	actionButtonIconContModify.appendChild(actionButtonIconModify);
	
	tableCellName.className = "has-text-centered";
	tableCellPower.className = "has-text-centered is-hidden-mobile";
	tableCellIsHardwired.className = "has-text-centered is-hidden-mobile";
	tableCellSigQty.className = "has-text-centered is-hidden-mobile";
	tableCellDateCreation.className = "has-text-centered is-hidden-mobile";
	tableCellDateModification.className = "has-text-centered is-hidden-mobile";
	tableCellActions.className = "has-text-right";
	
	actionButtonModify.className = "button is-small is-info";
	actionButtonIconContModify.className = "icon is-small";
	actionButtonIconModify.className = "mdi mdi-18px mdi-file-edit";
	
	actionButtonModify.onclick = openEditApplianceModal.bind(null, item.id);
	
	tableNewRow.className = item.is_active ? "" : "has-text-danger";
	
	tableCellID.innerText = item.id;
	tableCellName.innerText = item.name;
	tableCellPower.innerText = (item.max_time_on < 60) ? (item.max_time_on + " m") : (Math.round(item.max_time_on / 60) + " h");
	tableCellIsHardwired.innerHTML = item.is_hardwired ? "Sim" : "Não";
	tableCellSigQty.innerText = item.signature_qty;
	tableCellDateCreation.innerText = new Date(item.creation_date * 1000).toLocaleString("pt-BR");
	tableCellDateModification.innerText = new Date(item.modification_date * 1000).toLocaleString("pt-BR");
}

function updateApplianceTable() {
	var inactiveFilterCheckbox = document.getElementById("inactive-filter-checkbox");
	var applianceTable = document.getElementById("appliance-tbody");
	
	if(typeof window.smceeApplianceList != "object")
		return;
	
	while(applianceTable.lastChild)
		applianceTable.removeChild(applianceTable.lastChild);
	
	for(applianceItem of window.smceeApplianceList.values()) {
		if(inactiveFilterCheckbox !== null && inactiveFilterCheckbox.checked && !applianceItem.is_active)
			continue;
		
		applianceTableAddItem(applianceItem);
	}
}

function validateApplianceForm() {
	var fieldName = document.getElementById("input-appliance-name");
	var fieldIsActive = document.getElementById("input-appliance-is-active");
	var fieldIsHardwired = document.getElementById("input-appliance-is-hardwired");
	
	fieldName.classList.remove("is-danger");
	
	if(fieldName.value.length < 3 || fieldName.value.length > 32) {
		fieldName.classList.add("is-danger");
		return false;
	}
	
	return true;
}

function submitNewAppliance() {
	var xhrSubmitNewAppliance;
	var newAppliance;
	
	if(!validateApplianceForm())
		return;
	
	newAppliance = {
			"name": document.getElementById("input-appliance-name").value,
			"is_active": document.getElementById("input-appliance-is-active").checked,
			"max_time_on": Number(document.getElementById("input-appliance-max-time-on").value),
			"is_hardwired": document.getElementById("input-appliance-is-hardwired").checked
	};
	
	document.getElementById("button-submit-appliance").classList.add("is-loading");
	
	xhrSubmitNewAppliance = new XMLHttpRequest()
	
	xhrSubmitNewAppliance.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			if(responseObject.result === "success" && typeof responseObject.appliance_id == "number") {
				newAppliance.id = responseObject.appliance_id;
				newAppliance.signature_qty = 0;
				newAppliance.modification_date = newAppliance.creation_date = Math.round(new Date() / 1000);
				
				window.smceeApplianceList.set(responseObject.appliance_id, newAppliance);
				updateApplianceTable();
				
				modalCloseAll();
				
			} else if(responseObject.result === "failed") {
				alert("Erro ao salvar novo aparelho.");
			}
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to submit new appliance. Status: " + this.status);
		}
		
		document.getElementById("button-submit-appliance").classList.remove("is-loading");
	};
	
	xhrSubmitNewAppliance.open("POST", window.smceeApiUrlBase + "appliances", true);
	
	xhrSubmitNewAppliance.timeout = 2000;
	
	xhrSubmitNewAppliance.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrSubmitNewAppliance.send(JSON.stringify(newAppliance));
}

function submitUpdateAppliance(applianceID) {
	var xhrUpdateAppliance;
	var updatedAppliance;
	
	if(!validateApplianceForm())
		return;
	
	updatedAppliance = {
			"name": document.getElementById("input-appliance-name").value,
			"is_active": document.getElementById("input-appliance-is-active").checked,
			"max_time_on": Number(document.getElementById("input-appliance-max-time-on").value),
			"is_hardwired": document.getElementById("input-appliance-is-hardwired").checked
	};
	
	document.getElementById("button-submit-appliance").classList.add("is-loading");
	
	xhrUpdateAppliance = new XMLHttpRequest()
	
	xhrUpdateAppliance.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			if(responseObject.result === "success") {
				let originalAppliance = window.smceeApplianceList.get(applianceID);
				
				updatedAppliance.id = applianceID;
				updatedAppliance.signature_qty = originalAppliance.signature_qty;
				updatedAppliance.creation_date = originalAppliance.creation_date;
				updatedAppliance.modification_date = Math.round(new Date() / 1000);
				
				window.smceeApplianceList.set(applianceID, updatedAppliance);
				
				updateApplianceTable();
				
				modalCloseAll();
			}
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to submit updated appliance. Status: " + this.status);
		}
		
		document.getElementById("button-submit-appliance").classList.remove("is-loading");
	};
	
	xhrUpdateAppliance.open("PUT", window.smceeApiUrlBase + "appliances/" + applianceID, true);
	
	xhrUpdateAppliance.timeout = 2000;
	
	xhrUpdateAppliance.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrUpdateAppliance.send(JSON.stringify(updatedAppliance));
	
}
