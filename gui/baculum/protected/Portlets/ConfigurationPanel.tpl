<div id="<%=$this->getParent()->getID()%>configuration" class="configuration">
	<div id="configuration-window-container" class="configuration-window-container">
		<com:TActivePanel ID="ConfigurationWindowBox" CssClass="configuration-window-content">
			<com:TImageButton ImageUrl="<%=$this->getPage()->getTheme()->getBaseUrl()%>/icon_close.png" ImageAlign="right" Attributes.onclick="$('<%=$this->getParent()->getID()%>configuration').hide(); return false;" ToolTip="<%[ Close ]%>" />
			<com:TContentPlaceHolder ID="ConfigurationWindowContent" />
		</com:TActivePanel>
	</div>
</div>
