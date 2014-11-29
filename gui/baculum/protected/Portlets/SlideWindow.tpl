<div id="<%=$this->getParent()->getID()%>-slide-window-container" class="slide-window-container" style="display: none">
	<div id="<%=$this->getParent()->getID()%>-slide-window-progress" class="slide-window-progress"></div>
	<div class="slide-window-content">
		<com:TContentPlaceHolder ID="SlideWindowContent" />
		<div id="<%=$this->getParent()->getID()%>-slide-window-toolbar" class="slide-window-toolbar" style="display: none">
			<com:TImageButton ImageUrl="<%=$this->getPage()->getTheme()->getBaseUrl()%>/close.png" Style="margin: 5px 5px 0 0;float: right;" Attributes.onclick="SlideWindow.getObj('<%=$this->getParent()->getID()%>').toggleToolbar(); return false;" Attributes.alt="<%[ Close ]%>" ToolTip="<%[ Close ]%>" />
			<table>
				<tr>
					<td><%[ Limit: ]%></td>
					<td><com:TActiveDropDownList ID="Limit" ClientSide.OnComplete="SlideWindow.getObj('<%=$this->getParent()->getID()%>').setElementsCount();" /></td>
				</tr>
				<tr>
					<td><%[ Search: ]%></td>
					<td><com:TActiveTextBox ID="Search" /></td>
				</tr>
				<tr>
					<td><%[ View mode: ]%></td>
					<td><com:TLabel ForControl="Simple" Text="<%[ simple ]%>" /> <com:TActiveRadioButton ID="Simple" GroupName="views" ActiveControl.CallbackParameter="simple" /> <com:TLabel ForControl="Details" Text="<%[ details ]%>" /><com:TActiveRadioButton ID="Details" GroupName="views" ActiveControl.CallbackParameter="details" /></td>
				</tr>
			</table>
		</div>
		<div id="<%=$this->getParent()->getID()%>-slide-window-actions" class="slide-window-actions" style="display: none">
			<table>
				<tr>
					<td><%[ Action: ]%></td>
					<td>
						<com:TDropDownList ID="Actions"/>
						<com:TRequiredFieldValidator
							ID="ActionsValidator"
							ValidationGroup="ActionsGroup<%=$this->ApplyAction->ClientID%>"
							ControlToValidate="Actions"
							InitialValue="NoAction"
							ErrorMessage="Please select action."
							ClientSide.ObserveChanges="false"
							ClientSide.OnValidationSuccess="<%=$this->ApplyAction->ClientID%>_actions_func()"
						 />
					</td>
				</tr>
				<tr>
					<td></td>
					<td>
						<com:BActiveButton ID="ApplyAction" Text="<%[ Apply ]%>" OnClick="action" ValidationGroup="ActionsGroup<%=$this->ApplyAction->ClientID%>" Attributes.onclick="return SlideWindow.getObj('<%=$this->getParent()->getID()%>').checked.length > 50 ? confirm('Warning!\n\nYou checked over 50 elements.\nPlease note, that in case more time consuming actions, web browser request may timed out after 30 seconds. It this case action will be canceled and in consequence the action may not touch all selected elements.\n\nAre you sure you want to continue?') : true;" ClientSide.OnSuccess="<%=$this->getParent()->getID()%>_refresh_window_func();" />
						<script type="text/javascript">
							var <%=$this->ApplyAction->ClientID%>_actions_func = function() {
								var console_visible = $('<%=$this->getPage()->Console->ConsoleContainer->ClientID%>').visible();
								if (console_visible === false) {
									$('console_launcher').click();
								} else {
									window.scrollTo(0, document.body.scrollHeight);
								}
								SlideWindow.getObj('<%=$this->getParent()->getID()%>').hideActions();
							}
						</script>
					</td>
				</tr>
				<tr>
					<td></td>
					<td><a href="javascript:SlideWindow.getObj('<%=$this->getParent()->getID()%>').markAllChecked(false);"><%[ unmark all and close ]%></a></td>
				</tr>
			</table>
		</div>
	</div>
	<div class="slide-window-bar">
		<div id="<%=$this->getParent()->getID()%>-slide-window-title" class="slide-window-bar-title"><%=$this->getParent()->getWindowTitle()%><span></span></div>
		<div id="<%=$this->getParent()->getID()%>-slide-window-close" title="Close the window" class="slide-window-close"></div>
		<div id="<%=$this->getParent()->getID()%>-slide-window-fullsize" title="Change the window size" class="slide-window-fullsize"></div>
		<div id="<%=$this->getParent()->getID()%>-slide-window-tools" title="<%[ Switch the window view (normal/details) ]%>" class="slide-window-sort"></div>
		<com:TCallback ID="DetailView" OnCallback="switchView">
			<prop:ClientSide.OnLoading>
				$('<%=$this->getParent()->getID()%>-slide-window-progress').setStyle({'display': 'block'});
			</prop:ClientSide.OnLoading>
			<prop:ClientSide.OnComplete>
				$('<%=$this->getParent()->getID()%>-slide-window-progress').setStyle({'display': 'none'});
				SlideWindow.getObj('<%=$this->getParent()->getID()%>').setLoadRequest();
			</prop:ClientSide.OnComplete>
		</com:TCallback>
			<script type="text/javascript">
				<%=$this->getParent()->getID()%>_refresh_window_func = function() {
						var request = <%= $this->DetailView->ActiveControl->Javascript %>;
						request.dispatch();
					}
				$$('input[id=<%=$this->Simple->ClientID%>], input[id=<%=$this->Details->ClientID%>], select[id=<%=$this->Limit->ClientID%>]').each(function(el) {
					el.observe('change', <%=$this->getParent()->getID()%>_refresh_window_func);
				});
			</script>
	</div>
</div>
<script type="text/javascript">
	var id = '<%=$this->getParent()->getID()%>';

	var windowObj = new SlideWindowClass(id, {
		'showId': '<%=$this->getParent()->getButtonID()%>',
		'hideId': id + '-slide-window-close',
		'fullSizeId': id + '-slide-window-fullsize',
		'search': '<%=$this->Search->ClientID%>'}
	);
	var configWindowObj = new ConfigurationWindowClass(id);

	SlideWindow.registerObj(id, windowObj);
	ConfigurationWindow.registerObj(id, configWindowObj);
	SlideWindow.getObj(id).setConfigurationObj(configWindowObj);
</script>
