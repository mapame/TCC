window.onload = function() {
	authCheckAccessKey(initPage, redirectToLogin);
	addEventsNavbarBurgers();
}

function initPage() {
	userInfoFetch(function() {navbarPopulateItems("main-menu");});
	
	document.getElementById("button-discard-config").onclick = modalCloseAll;
	
	fetchConfigList();
}

function openEditConfigModal(configKey) {
	var config = window.smceeConfigList.get(configKey);
		
	document.getElementById("config-name").innerText = config.name;
	document.getElementById("config-key").innerText = configKey;
	document.getElementById("config-description").innerText = config.description;
	document.getElementById("input-config-value").value = config.value;
	
	document.getElementById("button-submit-config").onclick = submitConfigValue.bind(null, configKey);
	
	modalOpen("modal-edit-config");
}

function configsTableAddItem(item) {
	var tableNewRow = document.createElement("tr");
	var tableCellName = document.createElement("th");
	var tableCellValue = document.createElement("td");
	var tableCellDateModification = document.createElement("td");
	var tableCellActions = document.createElement("td");
	var actionButton = document.createElement("button");
	var actionButtonIconContainer = document.createElement("span");
	var actionButtonIcon = document.createElement("i");
	
	document.getElementById("configs-tbody").appendChild(tableNewRow);
	
	tableNewRow.appendChild(tableCellName);
	tableNewRow.appendChild(tableCellValue);
	tableNewRow.appendChild(tableCellDateModification);
	tableNewRow.appendChild(tableCellActions);
	
	tableCellActions.appendChild(actionButton);
	actionButton.appendChild(actionButtonIconContainer);
	actionButtonIconContainer.appendChild(actionButtonIcon);
	
	tableCellName.className = "is-vcentered";
	tableCellValue.className = "has-text-centered is-vcentered";
	tableCellDateModification.className = "has-text-centered is-vcentered is-hidden-mobile";
	tableCellActions.className = "has-text-right is-vcentered";
	
	actionButton.className = "button is-small is-info";
	actionButtonIconContainer.className = "icon is-small";
	actionButtonIcon.className = "mdi mdi-18px mdi-file-edit";
	
	actionButton.onclick = openEditConfigModal.bind(null, item.key);
	
	tableCellName.innerText = item.name;
	tableCellValue.innerText = item.value;
	tableCellDateModification.innerText = new Date(item.modification_date * 1000).toLocaleString("pt-BR");
}

function updateConfigTable() {
	var inactiveFilterCheckbox = document.getElementById("inactive-filter-checkbox");
	var configsTableBody = document.getElementById("configs-tbody");
	
	if(typeof window.smceeConfigList != "object")
		return;
	
	while(configsTableBody.lastChild)
		configsTableBody.removeChild(configsTableBody.lastChild);
	
	for(const configItem of window.smceeConfigList.values())
		configsTableAddItem(configItem);
}

function fetchConfigList() {
	var xhrConfigList = new XMLHttpRequest();
	
	xhrConfigList.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			window.smceeConfigList = new Map();
			
			for(const configItem of responseObject)
				window.smceeConfigList.set(configItem.key, configItem);
			
			updateConfigTable();
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch config list. Status: " + this.status);
		}
	}
	
	xhrConfigList.open("GET", window.smceeApiUrlBase + "configs");
	
	xhrConfigList.timeout = 2000;
	
	xhrConfigList.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrConfigList.send();
}

function submitConfigValue(configKey) {
	var xhrChangeConfig;
	var configValue = document.getElementById("input-config-value").value;
	
	xhrChangeConfig = new XMLHttpRequest()
	
	document.getElementById("button-submit-config").classList.add("is-loading");
	
	xhrChangeConfig.onload = function() {
		if(this.status === 200) {
			let updatedConfig = window.smceeConfigList.get(configKey);
			
			updatedConfig.value = configValue;
			updatedConfig.modification_date = Math.round(new Date() / 1000);
			
			window.smceeConfigList.set(configKey, updatedConfig);
			
			updateConfigTable();
			
			modalCloseAll();
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to submit new configuration value. Status: " + this.status);
		}
		
		document.getElementById("button-submit-config").classList.remove("is-loading");
	};
	
	xhrChangeConfig.open("PUT", window.smceeApiUrlBase + "configs/" + configKey, true);
	
	xhrChangeConfig.timeout = 2000;
	
	xhrChangeConfig.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrChangeConfig.send(JSON.stringify(configValue));
	
}
