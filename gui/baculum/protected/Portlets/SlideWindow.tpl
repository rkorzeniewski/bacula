<div id="<%=$this->UniqueID%>-slide-window-container" class="slide-window-container" style="display: none">
	<div id="<%=$this->UniqueID%>-slide-window-progress" class="slide-window-progress"></div>
	<div class="slide-window-content">
		<com:TContentPlaceHolder ID="SlideWindowContent" />
		<div id="<%=$this->UniqueID%>-slide-window-toolbar" class="slide-window-toolbar" style="display: none">
			<com:TImageButton ImageUrl="<%=$this->getPage()->getTheme()->getBaseUrl()%>/close.png" Style="margin: 5px 5px 0 0;float: right;" Attributes.onclick="<%=$this->ShowID%>SlideWindow.toggleToolbar(); return false;" Attributes.alt="<%[ Close ]%>" ToolTip="<%[ Close ]%>" />
			<table>
				<tr>
					<td><%[ Limit: ]%></td><td><com:TActiveDropDownList ID="Limit" ClientSide.OnComplete="<%=$this->ShowID%>SlideWindow.setElementsCount();" /></td>
				</tr>
				<tr>
					<td><%[ Search: ]%></td><td><com:TActiveTextBox ID="Search" /></td>
				</tr>
				<tr>
					<td><%[ View mode: ]%></td><td><com:TLabel ForControl="Simple" Text="<%[ simple ]%>" /> <com:TActiveRadioButton ID="Simple" GroupName="views" ActiveControl.CallbackParameter="simple" /> <com:TLabel ForControl="Details" Text="<%[ details ]%>" /><com:TActiveRadioButton ID="Details" GroupName="views" ActiveControl.CallbackParameter="details" /></td>
				</tr>
			</table>
		</div>
	</div>
	<div class="slide-window-bar">
		<div id="<%=$this->UniqueID%>-slide-window-title" class="slide-window-bar-title"><%=$this->getParent()->windowTitle%><span></span></div>
		<div id="<%=$this->UniqueID%>-slide-window-close" title="Close the window" class="slide-window-close"></div>
		<div id="<%=$this->UniqueID%>-slide-window-fullsize" title="Change the window size" class="slide-window-fullsize"></div>
		<div id="<%=$this->UniqueID%>-slide-window-tools" title="<%[ Switch the window view (normal/details) ]%>" class="slide-window-sort"></div>
		<com:TCallback ID="DetailView" OnCallback="switchView">
			<prop:ClientSide.OnLoading>
				$('<%=$this->UniqueID%>-slide-window-progress').setStyle({'display': 'block'});
			</prop:ClientSide.OnLoading>
			<prop:ClientSide.OnComplete>
				$('<%=$this->UniqueID%>-slide-window-progress').setStyle({'display': 'none'});
				<%=$this->ShowID%>SlideWindow.setLoadRequest();
			</prop:ClientSide.OnComplete>
		</com:TCallback>
			<script type="text/javascript">
				$$('input[id=<%=$this->Simple->ClientID%>], input[id=<%=$this->Details->ClientID%>], select[id=<%=$this->Limit->ClientID%>]').each(function(el) { 
					el.observe('change', function() {
						var request = <%= $this->DetailView->ActiveControl->Javascript %>;
						request.dispatch();
					});
				});
			</script>
	</div>
</div>
<script type="text/javascript">
	var <%=$this->ShowID%>SlideWindow = new SlideWindowClass('<%=$this->UniqueID%>', {'showId' : '<%=$this->ShowID%>', 'hideId': '<%=$this->UniqueID%>-slide-window-close', 'fullSizeId' : '<%=$this->UniqueID%>-slide-window-fullsize', 'search' : '<%=$this->Search->ClientID%>'});
</script>
