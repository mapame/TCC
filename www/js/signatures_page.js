window.onload = function() {
	authCheckAccessKey(initPage, redirectToLogin);
	addEventsNavbarBurgers();
	
	window.smceePowerDataHours = 4;
}

function initPage() {
	userInfoFetch(function() {navbarPopulateItems("main-menu");});
	
	document.getElementById("button-add-signatures").onclick = openAddSignatureModal;
	document.getElementById("button-delete-signatures").onclick = openDeleteSignatureModal;
	
	document.getElementById("appliance-filter-select").onchange = updateSignatureTable;
	document.getElementById("select-all-checkbox").onchange = toggleAllCheckboxes;
	
	document.getElementById("button-more-hours").onclick = showMoreHours;
	document.getElementById("button-submit-signatures").onclick = submitSignatures;
	document.getElementById("button-discard-signatures").onclick = modalCloseAll;
	
	document.getElementById("button-confirm-del-signatures").onclick = startSignatureDeletion;
	document.getElementById("button-cancel-del-signatures").onclick = modalCloseAll;
	
	window.smceeNewSignaturePowerChart = new Dygraph(document.getElementById("signature-power-chart"), [[0, null]], {
		labels: ["Hora", "Potência"],
		xValueParser: function(x) {return x;},
		drawPoints: false,
		legend: "never",
		axes: {
			y: {
				axisLabelFormatter: function(x) { return x + " W"; }
			}
		},
		annotationClickHandler: eventClickHandler
	});
	
	fetchPowerData(window.smceePowerDataHours * 3600);
	fetchApplianceList(function() { fetchSignatures(); updateApplianceSelects(); });
}

function showMoreHours() {
	if(window.smceePowerDataHours + 8 > 12) {
		this.disabled = true;
		document.getElementById("button-more-hours-label").innerText = "";
	} else {
		this.classList.add("is-loading");
		document.getElementById("button-more-hours-label").innerHTML = "&nbsp;(" + (window.smceePowerDataHours + 8) + "h)";
	}
	
	window.smceePowerDataHours += 4;
	
	fetchPowerData(window.smceePowerDataHours * 3600);
}

function toggleAllCheckboxes() {
	var checkall = document.getElementById("select-all-checkbox").checked;
	
	for(checkboxItem of document.getElementsByClassName("signature-checkbox"))
		checkboxItem.checked = checkall;
}

function updateApplianceSelects() {
	var applianceFilterSelect = document.getElementById("appliance-filter-select");
	var newSignatureApplianceSelect = document.getElementById("new-signature-appliance-select");
	var selectOption;
	
	if(typeof window.smceeApplianceList != "object" || applianceFilterSelect === null)
		return;
	
	while(applianceFilterSelect.childElementCount > 1)
		applianceFilterSelect.removeChild(applianceFilterSelect.lastChild);
	
	while(newSignatureApplianceSelect.childElementCount > 1)
		newSignatureApplianceSelect.removeChild(newSignatureApplianceSelect.lastChild);
	
	for(applianceItem of window.smceeApplianceList.values()) {
		selectOption = document.createElement("option");
		selectOption.innerText = applianceItem.name + (applianceItem.is_active ? "" : " (inativo)");
		selectOption.value = applianceItem.id;
		
		applianceFilterSelect.appendChild(selectOption);
		newSignatureApplianceSelect.appendChild(selectOption.cloneNode(true));
	}
}

function openAddSignatureModal() {
	window.smceeSelectedEvents = new Set();
	
	document.getElementById("new-signature-appliance-select").value = 0;
	document.getElementById("new-signature-appliance-select").parentNode.classList.remove("is-danger");
	
	document.getElementById("button-submit-signatures").disabled = true;
	
	document.getElementById("submit-signatures-text").innerText = "Salvar";
	
	updateAnnotations();
	
	modalOpen("modal-add-signatures");
	
	window.smceeNewSignaturePowerChart.resetZoom();
	window.smceeNewSignaturePowerChart.resize();
}

function openDeleteSignatureModal() {
	window.smceeSignatureDeleteList = [];
	
	for(checkboxItem of document.getElementsByClassName("signature-checkbox"))
		if(checkboxItem.checked && typeof checkboxItem.dataset.signaturetimestamp == "string")
			window.smceeSignatureDeleteList.push(Number(checkboxItem.dataset.signaturetimestamp));
	
	if(window.smceeSignatureDeleteList.length < 1)
		return;
	
	window.smceeSignatureDeleteQty = window.smceeSignatureDeleteList.length;
	
	document.getElementById("modal-title-delete-signatures").innerText = "Deletar " + smceeSignatureDeleteList.length + " assinaturas?";
	
	document.getElementById("signature-delete-progress").innerText = "0%";
	document.getElementById("signature-delete-progress").value = "0";
	
	document.getElementById("button-confirm-del-signatures").classList.remove("is-loading");
	document.getElementById("button-cancel-del-signatures").classList.remove("is-loading");
	
	modalOpen("modal-delete-signatures");
}

function startSignatureDeletion() {
	document.getElementById("button-confirm-del-signatures").classList.add("is-loading");
	document.getElementById("button-cancel-del-signatures").classList.add("is-loading");
	
	submitDeleteSignature();
	
	
}

function signatureTableAddItem(item) {
	var applianceName = (window.smceeApplianceList.has(item.appliance_id) ? window.smceeApplianceList.get(item.appliance_id).name : null);
	
	var tableNewRow = document.createElement("tr");
	var tableCellCheckbox = document.createElement("th");
	var tableCellDate = document.createElement("td");
	var tableCellAppliance = document.createElement("td");
	var tableCellTotalP = document.createElement("td");
	var tableCellPeakP = document.createElement("td");
	var tableCellDuration = document.createElement("td");
	var tableCellP = document.createElement("td");
	var tableCellS = document.createElement("td");
	var tableCellQ = document.createElement("td");
	var phaseValueSeparator = document.createElement("hr");
	var rowCheckbox = document.createElement("input");
	var actionButtonDelete = document.createElement("button");
	var actionButtonIconContDelete = document.createElement("span");
	var actionButtonIconDelete = document.createElement("i");
	
	document.getElementById("signature-tbody").appendChild(tableNewRow);
	
	tableNewRow.appendChild(tableCellCheckbox);
	tableNewRow.appendChild(tableCellDate);
	tableNewRow.appendChild(tableCellAppliance);
	tableNewRow.appendChild(tableCellDuration);
	tableNewRow.appendChild(tableCellTotalP);
	tableNewRow.appendChild(tableCellPeakP);
	tableNewRow.appendChild(tableCellP);
	tableNewRow.appendChild(tableCellS);
	tableNewRow.appendChild(tableCellQ);
	
	tableCellCheckbox.appendChild(rowCheckbox);
	
	tableCellCheckbox.className = "has-text-centered is-vcentered";
	tableCellDate.className = "has-text-centered is-vcentered is-size-7";
	tableCellAppliance.className = "has-text-centered is-vcentered" + ((applianceName != null && applianceName.length > 15) ? " is-size-7" : "");
	tableCellTotalP.className = "has-text-centered is-vcentered";
	tableCellPeakP.className = "has-text-centered is-vcentered is-hidden-mobile";
	tableCellDuration.className = "has-text-centered is-vcentered is-hidden-mobile";
	tableCellP.className = "has-text-right is-vcentered is-hidden-mobile";
	tableCellS.className = "has-text-right is-vcentered is-hidden-mobile";
	tableCellQ.className = "has-text-right is-vcentered is-hidden-mobile";
	phaseValueSeparator.className = "my-0";
	
	rowCheckbox.type = "checkbox";
	rowCheckbox.className = "signature-checkbox";
	rowCheckbox.dataset.signaturetimestamp = item.timestamp;
	
	tableCellDate.innerText = new Date(item.timestamp * 1000).toLocaleString("pt-BR");
	tableCellAppliance.innerText = applianceName == null ? "(" + item.appliance_id + ")" : applianceName;
	tableCellTotalP.innerText = Math.round(item.delta_pt);
	tableCellPeakP.innerText = (item.delta_pt > 0) ? Math.round(item.peak_pt) : "-";
	tableCellDuration.innerText = item.duration;
	
	tableCellP.appendChild(document.createTextNode(Math.round(item.delta_p[0])));
	tableCellP.appendChild(phaseValueSeparator);
	tableCellP.appendChild(document.createTextNode(Math.round(item.delta_p[1])));
	
	tableCellS.appendChild(document.createTextNode(Math.round(item.delta_s[0])));
	tableCellS.appendChild(phaseValueSeparator.cloneNode(false));
	tableCellS.appendChild(document.createTextNode(Math.round(item.delta_s[1])));
	
	tableCellQ.appendChild(document.createTextNode(Math.round(item.delta_q[0])));
	tableCellQ.appendChild(phaseValueSeparator.cloneNode(false));
	tableCellQ.appendChild(document.createTextNode(Math.round(item.delta_q[1])));
}

function updateSignatureTable() {
	var applianceFilterSelect = document.getElementById("appliance-filter-select");
	var signatureTable = document.getElementById("signature-tbody");
	
	if(typeof window.smceeSignatures != "object")
		return;
	
	while(signatureTable.lastChild)
		signatureTable.removeChild(signatureTable.lastChild);
	
	for(signatureItem of window.smceeSignatures.values()) {
		if(applianceFilterSelect !== null && Number(applianceFilterSelect.value) != 0 && Number(applianceFilterSelect.value) != signatureItem.appliance_id)
			continue;
		
		signatureTableAddItem(signatureItem);
	}
	
	document.getElementById("select-all-checkbox").checked = false;
}

function fetchSignatures() {
	var xhrSignatures = new XMLHttpRequest();
	
	xhrSignatures.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			window.smceeSignatures = new Map();
			
			for(signature of responseObject)
				window.smceeSignatures.set(signature.timestamp, signature);
			
			updateSignatureTable();
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch signatures. Status: " + this.status);
		}
	}
	
	xhrSignatures.open("GET", window.smceeApiUrlBase + "appliances/signatures");
	
	xhrSignatures.timeout = 2000;
	
	xhrSignatures.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrSignatures.send();
}

function submitDeleteSignature() {
	var xhrDeleteSignature = new XMLHttpRequest();
	var timestamp = window.smceeSignatureDeleteList.pop();
	
	xhrDeleteSignature.onload = function() {
		if(this.status === 200 || this.status === 404) {
			window.smceeSignatures.delete(timestamp);
			
			if(window.smceeSignatureDeleteList.length > 0) {
				let deletionProgresss = (100 - (window.smceeSignatureDeleteList.length / window.smceeSignatureDeleteQty) * 100).toFixed(1);
				
				document.getElementById("signature-delete-progress").value = deletionProgresss;
				document.getElementById("signature-delete-progress").innerText = deletionProgresss + "%";
				
				submitDeleteSignature();
			} else {
				updateSignatureTable();
				modalCloseAll();
			}
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to delete signature. Status: " + this.status);
		}
	}
	
	xhrDeleteSignature.open("DELETE", window.smceeApiUrlBase + "appliances/signatures/" + timestamp);
	
	xhrDeleteSignature.timeout = 2000;
	
	xhrDeleteSignature.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrDeleteSignature.send();
}

function fetchPowerData(secondQty) {
	var xhrPowerData = new XMLHttpRequest();
	
	xhrPowerData.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			var lastTimestamp = null;
			
			window.smceePowerData = [];
			
			if(responseObject.length < 1)
				return;
			
			window.smceePowerData.push([new Date((responseObject[responseObject.length - 1][0] - secondQty - 1) * 1000), null]);
			
			for(pdItem of responseObject) {
				if(lastTimestamp !== null && pdItem[0] - lastTimestamp > 1)
					window.smceePowerData.push([new Date((lastTimestamp + 1) * 1000), null]);
				
				window.smceePowerData.push([new Date(pdItem[0] * 1000), pdItem[1]]);
				
				lastTimestamp = pdItem[0];
			}
			
			window.smceeNewSignaturePowerChart.updateOptions({'file' : window.smceePowerData});
			window.smceeNewSignaturePowerChart.resetZoom();
			
			fetchPowerEvents(secondQty);
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch power data. Status: " + this.status);
		}
	}
	
	xhrPowerData.open("GET", window.smceeApiUrlBase + "power?type=pt&last=" + secondQty);
	
	xhrPowerData.timeout = 2000;
	
	xhrPowerData.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrPowerData.send();
}

function fetchPowerEvents(secondQty) {
	var xhrPowerEvents = new XMLHttpRequest();
	
	xhrPowerEvents.onload = function() {
		if(this.status === 200) {
			var responseObject = JSON.parse(this.responseText);
			
			console.info("Loaded " + responseObject.length + " power events.");
			
			window.smceeLoadEvents = new Map();
			
			for(eventItem of responseObject)
				window.smceeLoadEvents.set(eventItem.timestamp, eventItem);
			
			document.getElementById("button-add-signatures").classList.remove("is-loading");
			document.getElementById("button-more-hours").classList.remove("is-loading");
			
			if(typeof window.smceeSelectedEvents == "object")
				updateAnnotations()
			
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to fetch power events. Status: " + this.status);
		}
	}
	
	xhrPowerEvents.open("GET", window.smceeApiUrlBase + "power/events?last=" + secondQty);
	
	xhrPowerEvents.timeout = 2000;
	
	xhrPowerEvents.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrPowerEvents.send();
}

function updateAnnotations() {
	var annotations = [];
	
	if(typeof window.smceeLoadEvents != "object")
		return;
	
	for(eventItem of window.smceeLoadEvents.values()) {
		let annotation = {
			series: "Potência",
			x: eventItem.timestamp * 1000,
			icon: (window.smceeSelectedEvents.has(eventItem.timestamp) ? "img/checkbox-marked-outline.png" : (window.smceeSignatures.has(eventItem.timestamp) ? "img/checkbox-blank-badge-outline.png" : "img/checkbox-blank-outline.png")),
			width: 16,
			height: 16,
			text: eventItem.raw_delta_pt.toFixed(1) + " W em " + eventItem.duration + " seg\n",
			tickColor: "black",
			tickWidth: 1,
			tickHeight: 10,
		};
		
		annotation.text += "\ndP: " + eventItem.delta_pt.toFixed(1) + " W (" + eventItem.delta_p[0].toFixed(1) + " | " + eventItem.delta_p[1].toFixed(1) + ")";
		annotation.text += "\nPpk: " + eventItem.peak_pt.toFixed(1) + " W";
		annotation.text += "\ndS(VA): " + eventItem.delta_s[0].toFixed(1) + " | " + eventItem.delta_s[1].toFixed(1);
		annotation.text += "\ndQ(VAr): " + eventItem.delta_q[0].toFixed(1) + " | " + eventItem.delta_q[1].toFixed(1);
		
		if(eventItem.time_gap > 0)
			annotation.text += "\nLacuna: " + eventItem.time_gap + " s";
		
		if(eventItem.state >= 3)
			annotation.text += "\n\nAparelho: " + window.smceeApplianceList.get(eventItem.pair_appliance_id).name + " (" + eventItem.pair_score + ")";
		
		if(eventItem.state >= 2) {
			annotation.text += "\n\nOutlier Score: " + eventItem.outlier_score.toFixed(2) + "\n";
			
			for(let idx = 0; idx < 3; idx++)
				if(eventItem.possible_appliances[idx] > 0)
					annotation.text += "\n" + (idx + 1) + "- " + window.smceeApplianceList.get(eventItem.possible_appliances[idx]).name;
		}
		
		annotations.push(annotation);
	}
	
	window.smceeNewSignaturePowerChart.setAnnotations(annotations);
}

function eventClickHandler(annotation, point, dygraph, event) {
	var timestamp = Math.round(annotation.x / 1000);
	
	if(typeof window.smceeSelectedEvents != "object")
		window.smceeSelectedEvents = new Set();
	
	if(window.smceeSelectedEvents.has(timestamp))
		window.smceeSelectedEvents.delete(timestamp);
	else
		window.smceeSelectedEvents.add(timestamp);
	
	if(window.smceeSelectedEvents.size > 0) {
		document.getElementById("submit-signatures-text").innerText = "Salvar (" + window.smceeSelectedEvents.size + ")";
		document.getElementById("button-submit-signatures").disabled = false;
	} else {
		document.getElementById("submit-signatures-text").innerText = "Salvar";
		document.getElementById("button-submit-signatures").disabled = true;
	}
	
	updateAnnotations();
}

function submitSignatures() {
	var applianceIdSelector = document.getElementById("new-signature-appliance-select");
	var applianceId = Number(applianceIdSelector.value);
	var xhrSubmitSignatures;
	var newSignatures = [];
	
	if(window.smceeSelectedEvents.size <= 0)
		return;
	
	applianceIdSelector.parentNode.classList.remove("is-danger");
	
	if(applianceId <= 0) {
		applianceIdSelector.parentNode.classList.add("is-danger");
		return;
	}
	
	document.getElementById("button-submit-signatures").classList.add("is-loading");
	
	for(signatureTimestamp of window.smceeSelectedEvents.values()) {
		let signatureLoadEvent = window.smceeLoadEvents.get(signatureTimestamp);
		
		newSignatures.push({
			timestamp: signatureLoadEvent.timestamp,
			delta_pt: signatureLoadEvent.delta_pt,
			peak_pt: signatureLoadEvent.peak_pt,
			delta_p: signatureLoadEvent.delta_p,
			delta_q: signatureLoadEvent.delta_q,
			delta_s: signatureLoadEvent.delta_s,
			duration: signatureLoadEvent.duration,
		});
	}
	
	xhrSubmitSignatures = new XMLHttpRequest()
	
	xhrSubmitSignatures.onload = function() {
		if(this.status === 200) {
			window.smceeSelectedEvents.clear();
			modalCloseAll();
			
			for(newSignature of newSignatures) {
				newSignature.appliance_id = applianceId;
				window.smceeSignatures.set(newSignature.timestamp, newSignature);
			}
			
			updateSignatureTable();
		} else if(this.status === 401) {
			redirectToLogin();
		} else {
			console.error("Failed to submit new signatures. Status: " + this.status);
		}
		
		document.getElementById("button-submit-signatures").classList.remove("is-loading");
	};
	
	xhrSubmitSignatures.open("POST", window.smceeApiUrlBase + "appliances/" + applianceId + "/signatures", true);
	
	xhrSubmitSignatures.timeout = 2000;
	
	xhrSubmitSignatures.setRequestHeader("Authorization", "Bearer " + localStorage.getItem("access_key"));
	
	xhrSubmitSignatures.send(JSON.stringify(newSignatures));
}
