<%@ MasterClass="Application.Portlets.ConfigurationPanel"%>
<com:TContent ID="ConfigurationWindowContent">
	<com:TActivePanel DefaultButton="Apply">
		<strong><%[ Client name: ]%> <com:TActiveLabel ID="ClientName" /><com:TActiveLabel ID="ClientIdentifier" Visible="false" /></strong><br />
		<com:TActiveLabel ID="ClientDescription" Style="font-style: italic; font-size: 12px"/>
		<hr />
		<com:TValidationSummary
			ID="ValidationSummary"
			CssClass="validation-error-summary"
			ValidationGroup="ClientGroup"
			AutoUpdate="true"
			Display="Dynamic"
			HeaderText="<%[ There is not possible to run selected action because: ]%>" />
		<div class="text small"><%[ Console status ]%></div>
		<div class="field-full" style="min-height: 172px">
			<com:TActiveTextBox ID="ShowClient" TextMode="MultiLine" CssClass="textbox-auto" Style="height: 162px; font-size: 11px;" ReadOnly="true" />
		</div>
		<div class="button">
			<com:BActiveButton ID="Status" Text="<%[ Status ]%>" ClientSide.OnSuccess="ConfigurationWindow.getObj('ClientWindow').progress(false);" OnClick="status" />
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="FileRetention" Text="<%[ File retention (in days): ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="FileRetention" MaxLength="14" AutoPostBack="false" CssClass="textbox-auto" Text="" />
				<com:TActiveCustomValidator ID="FileRetentionValidator" ValidationGroup="ClientGroup" ControlToValidate="FileRetention" ErrorMessage="<%[ File retention value must be positive integer or zero. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="fileRetentionValidator" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="JobRetention" Text="<%[ Job retention (in days): ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="JobRetention" MaxLength="14" AutoPostBack="false" CssClass="textbox-auto" Text="" />
				<com:TActiveCustomValidator ID="JobRetentionValidator" ValidationGroup="ClientGroup" ControlToValidate="JobRetention" ErrorMessage="<%[ Job retention value must be positive integer or zero. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="jobRetentionValidator" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="AutoPrune" Text="<%[ AutoPrune: ]%>" /></div>
			<div class="field"><com:TActiveCheckBox ID="AutoPrune" AutoPostBack="false" /></div>
		</div>
		<com:TCallback ID="ReloadClients" OnCallback="Page.ClientWindow.prepareData" ClientSide.OnComplete="SlideWindow.getObj('ClientWindow').setLoadRequest();" />
		<script type="text/javascript">
			var client_callback_func = function() {
				var mainForm = Prado.Validation.getForm();
				var callback = <%=$this->ReloadClients->ActiveControl->Javascript%>;
				if (Prado.Validation.managers[mainForm].getValidatorsWithError('ClientGroup').length == 0) {
					callback.dispatch();
				}
			}
		</script>
		<div class="button">
			<com:BActiveButton ValidationGroup="ClientGroup" OnClick="apply" CausesValidation="true" ID="Apply" Text="<%[ Apply ]%>" ClientSide.OnSuccess="ConfigurationWindow.getObj('ClientWindow').progress(false);client_callback_func();" />
		</div>
	</com:TActivePanel>
</com:TContent>
