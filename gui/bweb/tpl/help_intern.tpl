<br/>
<div class='titlediv'>
  <h1 class='newstitle'> __Help loading media (part 1/2)__</h1>
</div>
<div class="bodydiv">
__This tool will select the best candidates to load. You will be asked to make your selection on the next screen.__
  <form action="?" method='GET'>
   <table>
    <tr><td>__Pool:__</td>      
        <td><select name='pool' class='formulaire' multiple>
<TMPL_LOOP db_pools>
             <option selected><TMPL_VAR name></option>
</TMPL_LOOP>
           </select>
        </td>
    </tr>
    <tr><td>__Media Type:__</td>      
        <td><select name='mediatype' class='formulaire' multiple>
<TMPL_LOOP db_mediatypes>
             <option><TMPL_VAR mediatype></option>
</TMPL_LOOP>
           </select>
        </td>
    </tr>
    <tr><td>
    __Location:__ 
    </td><td><select name='location' class='formulaire'>
  <TMPL_LOOP db_locations>
      <option value='<TMPL_VAR location>'><TMPL_VAR location></option>
  </TMPL_LOOP>
    </select>
    </td>
    </tr>
    <tr>
        <td>__Expired:__</td> 
        <td> <input type='checkbox' name='expired' class='formulaire' 
                checked> </td>
    </tr>
    <tr>
        <td>__Number of media <br/> to load:__</td> 
        <td> <input type='text' name='limit' class='formulaire' 
                size='3' value='10'> </td>
    </tr>
    <tr>
        <td><button type="submit" class="bp" name='action' value='compute_intern_media' title='__Next__'> <img src='/bweb/next.png' alt=''>__Next__</button>
        </td><td/>
    </tr>
   </table>
   </form>
</div>
