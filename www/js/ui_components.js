function modalOpen(modalID) {
	if(typeof modalID == "string") {
		document.getElementById(modalID).classList.add("is-active");
		document.documentElement.classList.add("is-clipped");
	}
}

function modalCloseAll() {
	document.documentElement.classList.remove("is-clipped");
	
	for(modal of document.getElementsByClassName("modal"))
		modal.classList.remove("is-active");
}

function modalAddCloseEvents() {
	for(background of document.getElementsByClassName("modal-background"))
		background.onclick = modalCloseAll;
	
	for(background of document.getElementsByClassName("modal-close-button"))
		background.onclick = modalCloseAll;
}

function tabRegisterClickEvents() {
	for(tab of document.getElementsByClassName("tab"))
		tab.onclick = tabChange;
}

function tabChange(actionCallback) {
	if(this.classList.contains("is-active"))
		return;
	
	for(tab of document.getElementsByClassName("tab"))
		tab.classList.remove("is-active");
	
	this.classList.add("is-active");
	
	if(typeof actionCallback == "function")
		actionCallback();
	
	if(typeof this.dataset.targetid == "string") {
		for(tabContent of document.getElementsByClassName("tab-content"))
			tabContent.classList.add("is-hidden");
		
		document.getElementById(this.dataset.targetid).classList.remove("is-hidden");
	}
}
