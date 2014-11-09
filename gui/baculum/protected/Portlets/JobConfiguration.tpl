<%@ MasterClass="Application.Portlets.ConfigurationPanel"%>
<com:TContent ID="ConfigurationWindowContent">
	<com:TActivePanel DefaultButton="Run.ApplyChanges">
		<strong><%[ Job name: ]%> <com:TActiveLabel ID="JobName" /><com:TActiveLabel ID="JobID" Visible="false" /></strong>
		<hr />
		<com:TValidationSummary
			ID="ValidationSummary"
			CssClass="validation-error-summary"
			ValidationGroup="JobGroup"
			AutoUpdate="true"
			Display="Dynamic"
			/>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Level" Text="<%[ Level: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="Level" AutoPostBack="false" CssClass="textbox-auto" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Client" Text="<%[ Client: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="Client" AutoPostBack="false" CssClass="textbox-auto" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="FileSet" Text="<%[ FileSet: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="FileSet" AutoPostBack="false" CssClass="textbox-auto" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Pool" Text="<%[ Pool: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="Pool" AutoPostBack="false" CssClass="textbox-auto" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Storage" Text="<%[ Storage: ]%>" /></div>
			<div class="field">
				<com:TActiveDropDownList ID="Storage" AutoPostBack="false" CssClass="textbox-auto" />
			</div>
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Priority" Text="<%[ Priority: ]%>" /></div>
			<div class="field">
				<com:TActiveTextBox ID="Priority" CssClass="textbox-auto" AutoPostBack="false" />
				<com:TActiveCustomValidator ID="PriorityValidator" ValidationGroup="JobGroup" ControlToValidate="Priority" ErrorMessage="<%[ Priority value must be integer greather than 0. ]%>" ControlCssClass="validation-error" Display="None" OnServerValidate="priorityValidator" ClientSide.OnValidationError="IsInvalid<%=$this->getPage()->JobConfiguration->getMaster()->ClientID%> = true" />
			</div>
		</div>
		<com:TCallback ID="ReloadJobs" OnCallback="Page.JobWindow.prepareData" ClientSide.OnComplete="jobSlideWindowObj.setLoadRequest();" />
		<script type="text/javascript">
				function <%=$this->getPage()->JobConfiguration->getMaster()->ClientID%>reloadWindow() {
					var callback = <%= $this->ReloadJobs->ActiveControl->Javascript %>;
					if(typeof(IsInvalid<%=$this->getPage()->JobConfiguration->getMaster()->ClientID%>) == 'undefined') {
						callback.dispatch();
					}
					delete IsInvalid<%=$this->getPage()->JobConfiguration->getMaster()->ClientID%>;
				}
		</script>
		<div class="button">
			<com:Application.Portlets.BActiveButton ID="Status" Text="<%[ Job status ]%>" CausesValidation="false" /> 
			<com:TActiveLabel ID="DeleteButton"><com:Application.Portlets.BActiveButton ID="Delete" Text="<%[ Delete job ]%>" CausesValidation="false" /> </com:TActiveLabel>
			<com:TActiveLabel ID="CancelButton"><com:Application.Portlets.BActiveButton ID="Cancel" Text="<%[ Cancel job ]%>" CausesValidation="false" /> </com:TActiveLabel>
			<com:Application.Portlets.BActiveButton ID="Run" Text="<%[ Run job again ]%>" ValidationGroup="JobGroup" CausesValidation="true" />
		</div>
		<div class="text small"><%[ Console status ]%></div>
		<div class="field-full" style="min-height: 166px">
			<com:TActiveTextBox ID="Estimation" TextMode="MultiLine" CssClass="textbox-auto" Style="height: 145px" ReadOnly="true" />
		</div>
		<div class="line">
			<div class="text"><com:TLabel ForControl="Accurate" Text="<%[ Accurate: ]%>" /></div>
			<div class="field"><com:TActiveCheckBox ID="Accurate" AutoPostBack="false" /></div>
		</div>
		<div class="button">
			<com:Application.Portlets.BActiveButton ID="Estimate" Text="<%[ Estimate job ]%>" />
		</div>
	</com:TActivePanel>
</com:TContent>
