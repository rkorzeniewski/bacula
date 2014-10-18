<div id="<%=$this->ClientID%>configuration" class="configuration">
	<div id="<%=$this->ClientID%>configuration-window-container" class="configuration-window-container">
		<com:TActivePanel ID="ConfigurationWindowBox" CssClass="configuration-window-content">
			<com:TImageButton ImageUrl="<%=$this->getPage()->getTheme()->getBaseUrl()%>/icon_close.png" ImageAlign="right" Attributes.onclick="ConfigurationWindow<%=$this->ClientID%>.hide(); return false;" ToolTip="<%[ Close]%>" />
			<com:TContentPlaceHolder ID="ConfigurationWindowContent" />

		</com:TActivePanel>
	</div>
</div>
<script type="text/javascript">
	var ConfigurationWindow<%=$this->ClientID%> = new ConfigurationWindowClass('<%=$this->ClientID%>');
</script>
